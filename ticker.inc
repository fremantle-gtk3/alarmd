// -*- mode: c -*-
#ifdef TICKER
TICKER(int,      get_synced,     (void))
TICKER(time_t,   get_time,       (void))
TICKER(int,      set_time,       (time_t tick))
TICKER(time_t,   mktime,         (struct tm *tm, const char *tz))
TICKER(int,      get_timezone,   (char *s, size_t max))
TICKER(int,      get_tzname,     (char *s, size_t max))
TICKER(int,      set_timezone,   (const char *tz))
TICKER(int,      get_utc,        (struct tm *tm))
TICKER(int,      get_utc_ex,     (time_t tick, struct tm *tm))
TICKER(int,      get_local,      (struct tm *tm))
TICKER(int,      get_local_ex,   (time_t tick, struct tm *tm))
TICKER(int,      get_remote,     (time_t tick, const char *tz, struct tm *tm))
TICKER(int,      get_time_format,(char *s, size_t max))
TICKER(int,      set_time_format,(const char *fmt))
TICKER(int,      format_time,    (const struct tm *tm, const char *fmt, char *s, size_t max))
TICKER(int,      get_utc_offset, (const char *tz))
TICKER(int,      get_dst_usage,  (time_t tick, const char *tz))
TICKER(double,   diff,           (time_t t1, time_t t2))
TICKER(int,      get_time_diff,  (time_t tick, const char *tz1, const char *tz2))
TICKER(int,      set_autosync,   (int enable))
TICKER(int,      get_autosync,   (void))
TICKER(int,      is_net_time_changed,         (time_t *tick, char *s, size_t max))
TICKER(int,      activate_net_time,           (void))
TICKER(int,      is_operator_time_accessible, (void))
#undef TICKER
#endif