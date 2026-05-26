#include <time_utils.h>

void ms_sleep(uint64_t ms) {
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
#endif
}

/* chatte generated custom timegm for embedded platforms*/
#ifndef timegm
time_t timegm(struct tm *_tm) {
  static const int days_in_month[12] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
  int year = _tm->tm_year + 1900;
  int month = _tm->tm_mon;
  int day =
      _tm->tm_mday - 1; /* struct tm days start from 1, Unix epoch from 0 */

  /* Calculate days before current year */
  int days = (year - 1970) * 365 + ((year - 1969) / 4) - ((year - 1901) / 100) +
             ((year - 1601) / 400);
  /* Add days for previous months this year */
  for (int i = 0; i < month; ++i)
    days += days_in_month[i];
  /* Leap year adjustment */
  if (month > 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    days += 1;
  /* Add current day */
  days += day;
  /* Convert to seconds */
  return (time_t)((((days * 24 + _tm->tm_hour) * 60 + _tm->tm_min) * 60) +
                  _tm->tm_sec);
}
#endif /* timegm */

uint64_t SystemMonotonicMS() {
  long ms;
  time_t s;

  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  s = spec.tv_sec;
  ms = (spec.tv_nsec / 1000000);

  uint64_t result = s;
  result *= 1000;
  result += ms;

  return result;
}

/** Return amount of offset hours local time is from UTC */
/* WARNING: NOT TESTED, MIGHT BE SHIT */
int utc_offset_hours() {
  time_t now = time(NULL);
  struct tm *utc = gmtime(&now);    // UTC broken-down time
  utc->tm_isdst = -1;               // Let mktime determine DST
  time_t local_stamp = mktime(utc); // Interpret UTC as local
  double hours_offset = difftime(now, local_stamp) / 3600.0;
  return (int)hours_offset;
}

/* Helper for parsing iso formatted datetime string to time_t epoch
 * Format: "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d" */
time_t parse_iso_full_datetime_string_to_epoch(const char *_time_str) {
  struct tm tm = {0};
  int year, month, day, hour, min, sec, utc_hour, utc_min;
  char utc_direction;

  if (sscanf(_time_str, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", &year,
             &month, &day, &hour, &min, &sec, &utc_direction, &utc_hour,
             &utc_min) != 9)
    return (time_t)-1;

  if (utc_direction == '+')
    tm.tm_hour = hour - utc_hour;
  else if (utc_direction == '-')
    tm.tm_hour = hour + utc_hour;
  else // incorrect string
    return (time_t)-1;

  tm.tm_year = year - 1900; /* struct tm years since 1900 */
  tm.tm_mon = month - 1;    /* struct tm months are zero-based */
  tm.tm_mday = day;
  tm.tm_min = min;
  tm.tm_sec = sec;
  tm.tm_isdst = -1; /* Not considering daylight saving time */

  return timegm(&tm);
}

/* Helper for parsing iso8601 formatted datetime string to time_t epoch
 * Format: "%04d-%02d-%02dT%02d:%02d" */
time_t parse_iso_datetime_string_to_epoch(const char *_time_str) {
  struct tm tm = {0};
  int year, month, day, hour, min;

  if (sscanf(_time_str, "%4d-%2d-%2dT%2d:%2d", &year, &month, &day, &hour,
             &min) != 5)
    return (time_t)-1;

  tm.tm_year = year - 1900; /* struct tm years since 1900 */
  tm.tm_mon = month - 1;    /* struct tm months are zero-based */
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_isdst = -1; /* Not considering daylight saving time */

  return timegm(&tm);
}

bool time_is_at_or_after_hour(int _hour) {
  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);

  return (local.tm_hour > _hour) ||
         (local.tm_hour == _hour && local.tm_min >= 0);
}

time_t epoch_now_day() {
  time_t now = time(NULL);
  struct tm *today = gmtime(&now);

  today->tm_hour = 0;
  today->tm_min = 0;
  today->tm_sec = 0;

  return timegm(today);
}
time_t epoch_now_hour() {
  time_t now = time(NULL);
  struct tm *today = gmtime(&now);

  today->tm_min = 0;
  today->tm_sec = 0;

  return timegm(today);
}
time_t epoch_now_min() {
  time_t now = time(NULL);
  struct tm *today = gmtime(&now);

  today->tm_sec = 0;

  return timegm(today);
}

time_t epoch_now_day_utc(void) {
  time_t now = time(NULL);
  struct tm tm;

  gmtime_r(&now, &tm);

  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  return timegm(&tm);
}

int epoch_to_utc_date_string(time_t ts, char *out, size_t out_sz) {
  if (!out || out_sz < 11)
    return -1;

  struct tm tm_utc;

  if (gmtime_r(&ts, &tm_utc) == NULL)
    return -1;

  if (strftime(out, out_sz, "%Y-%m-%d", &tm_utc) != 10)
    return -1;

  return 0;
}
