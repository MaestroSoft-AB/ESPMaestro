#ifndef __TIME_UTILS_H__
#define __TIME_UTILS_H__

/* ******************************************************************* */
/* *************************** TIME UTILS **************************** */
/* ******************************************************************* */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif /*_WIN32*/

#define ISO_DATE_STRING_FULL "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d"
#define ISO_DATE_STRING_FULL_LEN 20
#define ISO_DATE_STRING "%04d-%02d-%02dT%02d:%02d"
#define ISO_DATE_STRING_LEN 16
#define ISO_DATE_STRING_DAY "%04d-%02d-%02d"
#define ISO_DATE_STRING_DAY_LEN 10

/* ---------------------- Interface ----------------------- */

/* chatte generated custom timegm for non-posix platforms */
#ifndef timegm
time_t timegm(struct tm *_tm);
#endif /* timegm */

void ms_sleep(uint64_t ms);

uint64_t SystemMonotonicMS();
/** Helper for parsing iso8601 formatted datetime string to time_t epoch */

/** Return amount of offset hours local time is from UTC */
int utc_offset_hours();

/* Helper for parsing iso8601 formatted datetime string to time_t epoch
 * Format: "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d" */
time_t parse_iso_full_datetime_string_to_epoch(const char *_time_str);
/* Helper for parsing iso formatted datetime string to time_t epoch
 * Format: "%04d-%02d-%02dT%02d:%02d%c%02d:%02d" */
time_t parse_iso_datetime_string_to_epoch(const char *_time_str);

/* Helper for parsing time_t epoch to iso formatted datetime string
 * Uses strdup, i.e return value needs to be freed from heap by caller
 * Format: "%04d-%02d-%02dT%02d:%02d" */
/* WARNING: NOT TESTED, MIGHT BE SHIT */
char *parse_epoch_to_iso_full_datetime_string(const time_t *_epoch,
                                              int _offset_hours);
/** Helper for parsing time_t epoch to iso8601 formatted datetime string
 * uses strdup, i.e return value needs to be freed from heap by caller */
char *parse_epoch_to_iso_datetime_string(const time_t *_epoch);
/** Helper for parsing time_t epoch to iso8601 formatted datetime string
 * uses strdup, i.e return value needs to be freed from heap by caller */
char *parse_epoch_to_iso_short_datetime_string(const time_t *_epoch);

bool time_is_at_or_after_hour(int _hour);

/* Get epoch with specified accuracy */
time_t epoch_now_day();
time_t epoch_now_hour();
time_t epoch_now_min();
time_t epoch_now_day_utc(void);
int epoch_to_utc_date_string(time_t ts, char *out, size_t out_sz);
/* -------------------------------------------------------- */

#endif
