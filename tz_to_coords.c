#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#include "str_vec.h"
#include "tz_to_coords.h"



static const char * const zoneinfo_paths[] = {
	"/usr/share/zoneinfo/zone1970.tab",
	"/usr/share/zoneinfo/zone.tab",
	"/usr/lib/zoneinfo/zone1970.tab",
	"/usr/lib/zoneinfo/zone.tab",
	NULL
};


char *get_local_tz_name(void) {
	// try env var TZ first
	char *env = getenv("TZ");
	if (env != NULL) return strdup(env);

	// try readlink on /etc/localtime in this case we use the path after /zoneinfo/
	char linkbuf[PATH_MAX + 1];
	ssize_t len = readlink("/etc/localtime", linkbuf, sizeof(linkbuf) - 1);
	if (len > 0) {
		linkbuf[len] = '\0';
		const char *needle = "/zoneinfo/";
		char *p = strstr(linkbuf, needle);
		if (p) {
			p += strlen(needle);
			return strdup(p);
		}
	}

	return NULL;
}


/* convert zone.tab coords string to decimal lat/lon
 * format: +DDMM+DDDMM or +DDMMSS+DDDMMSS
 * +4930-12310  -> 49.16, -123.10
 */

static int coords_to_decimal(const char *coord, double *out_lat, double *out_lon) {
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

	// parse lon (deg , min, optional sec)
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

int lookup_tz_coords(const char *tzname, double *out_lat, double *out_lon) {
	if (!tzname) return -1;

	struct str_vec paths;
	str_vec_init(&paths);

	// TZDIR paths if set
	const char *tzdir = getenv("TZDIR");
	if (tzdir && *tzdir) {
		char buf[PATH_MAX + 1];
		if (snprintf(buf, sizeof(buf), "%s/%s", tzdir, "zone1970.tab") < (int)sizeof(buf))
			str_vec_push(&paths, buf);
		if (snprintf(buf, sizeof(buf), "%s/%s", tzdir, "zone.tab") < (int)sizeof(buf))
			str_vec_push(&paths, buf);
	}

	// then add hardcoded paths
	for (const char * const *p = zoneinfo_paths; p && *p; ++p) {
		str_vec_push(&paths, *p);
	}

	char *line = NULL;
	size_t sz = 0;
	int result = -1;

	for (size_t i = 0; i < paths.len; ++i) {
		const char *path = paths.data[i];
		FILE *f = fopen(path, "r");
		if (!f) continue;

		while (getline(&line, &sz, f) != -1) {
			// check for comments and blank lines
			char *scan = line;
			while (*scan == ' ' || *scan == '\t') ++scan;
			if (*scan == '#' || *scan == '\0' || *scan == '\n' || *scan == '\r') continue;

			char *save = NULL;
			char *tok1 = strtok_r(line, " \t\r\n", &save); // country code
			if (!tok1) continue;
			char *tok2 = strtok_r(NULL, " \t\r\n", &save); // coords
			if (!tok2) continue;
			char *tok3 = strtok_r(NULL, " \t\r\n", &save); // tzname
			if (!tok3) continue;

			// strip again
			while (*tok3 == ' ' || *tok3 == '\t') tok3++;

			if (strcmp(tok3, tzname) == 0) {
				double lat, lon;
				if (coords_to_decimal(tok2, &lat, &lon) == 0) {
					*out_lat = lat;
					*out_lon = lon;
					result = 0;
				} else {
					result = -1;
				}
				break; // found, stop searching, all versions of theses files should have same data
			}
		}

		fclose(f);
		if (result != -1) break;
		free(line); line = NULL; sz = 0;
	}

	free(line);
	str_vec_free(&paths);
	return result;
}
