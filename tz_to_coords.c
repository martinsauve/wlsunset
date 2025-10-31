#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#include "tz_to_coords.h"



static const char *zoneinfo_paths[] = {
   "/usr/share/zoneinfo/zone1970.tab",
   "/usr/share/zoneinfo/zone.tab",
   "/usr/lib/zoneinfo/zone1970.tab",
   "/usr/lib/zoneinfo/zone.tab",
   NULL
};

// if contains a slash, use it TODO: some tz actually don't have slashes? figure out how to handle those
static bool is_valid_tz_name(const char *name)
{
   return name && strchr(name, '/') != NULL;
}

char *get_local_tz_name(void)
{
   char *env = getenv("TZ");
   if (env && is_valid_tz_name(env)) {
      return strdup(env);
   }

   FILE *f = fopen("/etc/localtime", "r");
   if (f) {
      char buf[256];
      if (fgets(buf, sizeof(buf), f)){
         size_t n = strlen(buf);
         // strip newline
         if (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
            buf[n-1] = '\0';
         }
         fclose(f);
         if (strlen(buf) > 0 && is_valid_tz_name(buf)) {
            return strdup(buf);
         }
      } else {
         fclose(f);
      }
   }

   // try readlink on /etc/localtime (some distros use a symlink), in this case we use the path after /zoneinfo/
   char linkbuf[PATH_MAX + 1];
   ssize_t len = readlink("/etc/localtime", linkbuf, PATH_MAX);
   if (len > 0) {
      linkbuf[len] = '\0';
      // TODO: verify this use case
      const char *needle = "/zoneinfo/";
      char *p = strstr(linkbuf, needle);
      if (p) {
         p += strlen(needle);
         if (is_valid_tz_name(p)) {
            return strdup(p);
         }
      }
   }

   return NULL;
}


/* convert zone.tab coords string to decimal lat/lon
 * format: +DDMM+DDDMM or +DDMMSS+DDDMMSS
 * +4930-12310  -> 49.16, -123.10
 */

static int coords_to_decimal(const char *coord, double *out_lat, double *out_lon)
{
   if (!coord || coord[0] == '\0') {
      return -1;
   }

   const char *p = coord + 1;
   while (*p && *p != '+' && *p != '-') p++;
   if (*p == '\0') return -1;

   const char lat_sign = coord[0];
   const char lon_sign = *p;
   const char *lat_str = coord + 1;
   size_t lat_len = p - lat_str;
   const char *lon_str = p + 1;
   size_t lon_len = strlen(lon_str);


   // parse lat (deg, min, optional sec)
   int lat_deg = 0, lat_min = 0, lat_sec = 0;
   if (lat_len == 4 || lat_len == 6) {
      char tmp[8];
      if (lat_len == 4) { // DD MM
         memcpy(tmp, lat_str, 2); tmp[2] = '\0';
         lat_deg = atoi(tmp);
         memcpy(tmp, lat_str + 2, 2); tmp[2] = '\0';
         lat_min = atoi(tmp);
         lat_sec = 0;
      } else { // DD MM SS
         memcpy(tmp, lat_str, 2); tmp[2] = '\0';
         lat_deg = atoi(tmp);
         memcpy(tmp, lat_str + 2, 2); tmp[2] = '\0';
         lat_min = atoi(tmp);
         memcpy(tmp, lat_str + 4, 2); tmp[2] = '\0';
         lat_sec = atoi(tmp);
      }
   } else {
      return -1;
   }

   // parse lon (deg (can be 3 digits) , min, optional sec)
   int lon_deg = 0, lon_min = 0, lon_sec = 0;
   if (lon_len == 5 || lon_len == 7) {
      char tmp[8];
      if (lon_len == 5) { // DDD MM
         memcpy(tmp, lon_str, 3); tmp[3] = '\0';
         lon_deg = atoi(tmp);
         memcpy(tmp, lon_str + 3, 2); tmp[2] = '\0';
         lon_min = atoi(tmp);
         lon_sec = 0;
      } else { // DDD MM SS
         memcpy(tmp, lon_str, 3); tmp[3] = '\0';
         lon_deg = atoi(tmp);
         memcpy(tmp, lon_str + 3, 2); tmp[2] = '\0';
         lon_min = atoi(tmp);
         memcpy(tmp, lon_str + 5, 2); tmp[2] = '\0';
         lon_sec = atoi(tmp);
      }
   } else {
      return -1;
   }

   //convert to decimal
   double lat = lat_deg + (lat_min / 60.0) + (lat_sec / 3600.0);
   double lon = lon_deg + (lon_min / 60.0) + (lon_sec / 3600.0);
   if (lat_sign == '-') lat = -lat;
   if (lon_sign == '-') lon = -lon;

   *out_lat = lat;
   *out_lon = lon;
   return 0;
}

// lookup timezone name in zone.tab or zone1970.tab

int lookup_tz_coords(const char *tzname, double *out_lat, double *out_lon)
{
   if (!tzname) return -1;

   // support for TZDIR env var, (nixOS seems to use it)
    const char *tzdir = getenv("TZDIR");
    if (tzdir && *tzdir) {
        const char *names[] = { "zone1970.tab", "zone.tab", NULL };
        char *line = NULL;
        size_t sz = 0;
        for (size_t i = 0; names[i]; ++i) {
            size_t need = strlen(tzdir) + 1 + strlen(names[i]) + 1;
            char *path = malloc(need);
            if (!path) continue;

            snprintf(path, need, "%s/%s", tzdir, names[i]);
            FILE *f = fopen(path, "r");
            free(path);

            if (!f) continue;

            while (getline(&line, &sz, f) != -1) {
                if (line[0] == '#' || line[0] == '\n') continue;
                char *save = NULL;
                char *tok1 = strtok_r(line, " \t\r\n", &save); // country code
                if (!tok1) continue;
                char *tok2 = strtok_r(NULL, " \t\r\n", &save); // coords
                if (!tok2) continue;
                char *tok3 = strtok_r(NULL, " \t\r\n", &save); // tzname
                if (!tok3) continue;
                while (*tok3 == ' ' || *tok3 == '\t') tok3++;
                if (strcmp(tok3, tzname) == 0) {
                    double lat, lon;
                    if (coords_to_decimal(tok2, &lat, &lon) == 0) {
                        *out_lat = lat;
                        *out_lon = lon;
                        free(line);
                        fclose(f);
                        return 0;
                    } else {
                        free(line);
                        fclose(f);
                        return -1;
                    }
                }
            }
            free(line);
            line = NULL;
            sz = 0;
            fclose(f);
        }
    }


   // use hardcoded paths if TZDIR is not set
   for (const char **pp = zoneinfo_paths; *pp; ++pp) {
      const char *path = *pp;
      FILE *f = fopen(path, "r");
      if (!f) continue;
      char *line = NULL;
      size_t sz = 0;
      while (getline(&line, &sz, f) != -1) {
         // TODO: handle lines starting with tabs or spaces before '#', seems fine as is because every system i've seen IANA provided zone.tab file, and those don't have leading spaces
         if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
         // tokenize by tab or spaces
         char *save = NULL;
         char *tok1 = strtok_r(line, " \t\r\n", &save); //country code
         if (!tok1) continue;
         char *tok2 = strtok_r(NULL, " \t\r\n", &save); //coords
         if (!tok2) continue;
         char *tok3 = strtok_r(NULL, " \t\r\n", &save); //tz name
         if (!tok3) continue;
         while (*tok3 == ' ' || *tok3 == '\t') tok3++;
         if (strcmp(tok3, tzname) == 0) {
            double lat, lon;
            if (coords_to_decimal(tok2, &lat, &lon) == 0) {
               *out_lat = lat;
               *out_lon = lon;
               free(line);
               fclose(f);
               return 0;
            } else {
               free(line);
               fclose(f);
               return -1;
            }
         }
      }
      free(line);
      fclose(f);
   }
   return -1;
}
