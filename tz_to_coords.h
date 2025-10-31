#ifndef _TZ_TO_COORDS_H
#define _TZ_TO_COORDS_H

#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>


char *get_local_tz_name(void);
int lookup_tz_coords(const char *tzname, double *out_lat, double *out_lon);

#endif
