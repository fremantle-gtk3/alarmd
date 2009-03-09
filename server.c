#include "alarmd_config.h"

#include "server.h"
#include "logging.h"
#include "queue.h"
#include "ticker.h"
#include "dbusif.h"
#include "xutil.h"
#include "hwrtc.h"
#include "serialize.h"

#include "ipc_statusbar.h"
#include "ipc_icd.h"
#include "ipc_exec.h"
#include "ipc_systemui.h"
#include "ipc_dsme.h"

#include "alarm_dbus.h"
#include "missing_dbus.h"
#include "systemui_dbus.h"
#include "clockd_dbus.h"
#include <clock-plugin-dbus.h>
#include <mce/dbus-names.h>

#include <dbus/dbus-glib-lowlevel.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#define SNOOZE_HIJACK_FIX 1  // changes meaning of eve->snooze_total
#define SNOOZE_ADJUST_FIX 1  // snooze triggers follow system time

/* ========================================================================= *
 * Local prototypes
 * ========================================================================= */

static void server_rethink_request(void);

#if 0

static int         running_in_scratchbox(void);
static char *      tm_repr(const struct tm *tm, const char *tz, char *buff, size_t size);

static time_t      ticker_bump_time(time_t base, time_t skip, time_t target);
static void        ticker_init_from_template(struct tm *trg, const struct tm *now, const struct tm *tpl);

static gboolean server_queue_save_cb(gpointer data);
static gboolean server_queue_idle_cb(gpointer data);
static void     server_queue_cancel_save(void);
static void     server_queue_request_save(void);
static void     server_queue_forced_save(void);

static void server_queuestate_reset(void);
static void server_queuestate_invalidate(void);
static void server_queuestate_init(void);
static void server_queuestate_sync(void);

static void server_timestate_read(void);
static void server_timestate_sync(void);
static int  server_timestate_changed(void);
static void server_timestate_init(void);

static unsigned server_state_get(void);
static void     server_state_clr(unsigned clr);
static void     server_state_set(unsigned clr, unsigned set);
static void     server_state_init(void);
static void     server_state_sync(void);
static const char *server_state_get_systemui_service(void);

static unsigned server_action_get_type(alarm_action_t *action);
static unsigned server_action_get_when(alarm_action_t *action);
static int      server_action_do_snooze(alarm_event_t *event, alarm_action_t *action);
static int      server_action_do_desktop(alarm_event_t *event, alarm_action_t *action);
static int      server_action_do_actdead(alarm_event_t *event, alarm_action_t *action);
static int      server_action_do_exec(alarm_event_t *event, alarm_action_t *action);
static int      server_action_do_dbus(alarm_event_t *event, alarm_action_t *action);
static int      server_action_do_all(alarm_event_t *event, alarm_action_t *action);

static void        server_event_do_state_actions(alarm_event_t *eve, unsigned when);
static void        server_event_do_response_actions(alarm_event_t *eve);
static const char *server_event_get_tz(alarm_event_t *self);
static unsigned    server_event_get_boot_mask(alarm_event_t *eve);
static time_t      server_event_get_snooze(alarm_event_t *eve);
static int         server_event_is_snoozed(alarm_event_t *eve);
static int         server_event_get_buttons(alarm_event_t *self, int *vec, int size);
static time_t      server_event_get_next_trigger(time_t t0, time_t t1, alarm_event_t *self);
static int         server_event_set_trigger(alarm_event_t *self);

static gboolean    server_clock_source_is_stable(void);
static int         server_handle_systemui_ack(cookie_t *vec, int cnt);
static void        server_systemui_ack_cb(dbus_int32_t *vec, int cnt);
static int         server_handle_systemui_rsp(cookie_t cookie, int button);

static void     server_rethink_new(void);
static void     server_rethink_waitconn(void);
static void     server_rethink_queued(void);
static void     server_rethink_missed(void);
static void     server_rethink_postponed(void);
static void     server_rethink_triggered(void);
static void     server_rethink_waitsysui(void);
static void     server_rethink_sysui_req(void);
static void     server_rethink_sysui_ack(void);
static void     server_rethink_sysui_rsp(void);
static void     server_rethink_snoozed(void);
static void     server_rethink_served(void);
static void     server_rethink_recurring(void);
static void     server_rethink_deleted(void);
static void     server_rethink_timezone(void);
static void     server_rethink_back_in_time(void);
static void     server_rethink_forw_in_time(void);
static gboolean server_rethink_start_cb(gpointer data);
static void     server_rethink_request(void);

static gboolean server_wakeup_start_cb(gpointer data);
static void     server_cancel_wakeup(void);
static void     server_request_wakeup(time_t tmo);

static DBusMessage *server_handle_set_debug(DBusMessage *msg);
static DBusMessage *server_handle_snooze_get(DBusMessage *msg);
static DBusMessage *server_handle_snooze_set(DBusMessage *msg);
static DBusMessage *server_handle_event_add(DBusMessage *msg);
static DBusMessage *server_handle_event_update(DBusMessage *msg);
static DBusMessage *server_handle_event_del(DBusMessage *msg);
static DBusMessage *server_handle_event_query(DBusMessage *msg);
static DBusMessage *server_handle_event_get(DBusMessage *msg);
static DBusMessage *server_handle_event_ack(DBusMessage *msg);
static DBusMessage *server_handle_queue_ack(DBusMessage *msg);
static DBusMessage *server_handle_name_acquired(DBusMessage *msg);
static DBusMessage *server_handle_save_data(DBusMessage *msg);
static DBusMessage *server_handle_shutdown(DBusMessage *msg);
static DBusMessage *server_handle_name_owner_chaned(DBusMessage *msg);
static DBusMessage *server_handle_time_change(DBusMessage *msg);

static DBusHandlerResult server_session_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data);
static DBusHandlerResult server_system_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data);

static void server_icd_status_cb(int connected);

static int  server_init_session_bus(void);
static int  server_init_system_bus(void);

static void server_quit_session_bus(void);
static void server_quit_system_bus(void);
#endif

/* ========================================================================= *
 * Configuration
 * ========================================================================= */

/** Various constants affecting alarmd behaviour */
typedef enum alarmlimits
{
  /** If alarm event is triggered more than SERVER_MISSED_LIMIT
   *  seconds late, it will be handled as delayed */
  //SERVER_MISSED_LIMIT = 10,

  SERVER_MISSED_LIMIT = 59,

  /** The hardware /dev/rtc alarm interrupt is set this many
   *  seconds before the actual alarm time to give the system
   *  time to boot up to acting dead mode before the alarm
   *  time.
   */
  POWERUP_COMPENSATION_SECS = 60,

  /** The hardware /dev/rtc alarm interrupts are set at minimum
   *  this far from the current time.
   */
  ALARM_INTERRUPT_LIMIT_SECS = 60,

  /** Saving of alarm data is delayed until there is at least
   *  this long period of no more changes.
   *
   *  Note: data is saved immediately upon "save data" signal
   *        from dsme or when alarmd exits.
   */
  SERVER_QUEUE_SAVE_DELAY_MSEC = 1 * 1000,

  SERVER_POWERUP_BIT = 1<<31,
} alarmlimits;

/* ========================================================================= *
 * D-Bus Connections
 * ========================================================================= */

static DBusConnection *server_session_bus = NULL;
static DBusConnection *server_system_bus  = NULL;

/* ========================================================================= *
 * D-Bus signal masks
 * ========================================================================= */

// FIXME: is this in some include file?
#define DESKTOP_SERVICE "com.nokia.HildonDesktop.Home"

// FIXME: these are bound to have standard defs somewhere
#define DBUS_NAME_OWNER_CHANGED "NameOwnerChanged"
#define DBUS_NAME_ACQUIRED      "NameAcquired"

#define MATCH_STATUSAREA_CLOCK_OWNERCHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"STATUSAREA_CLOCK_SERVICE"'"

#define MATCH_SYSTEMUI_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"SYSTEMUI_SERVICE"'"

#define MATCH_FAKESYSTEMUI_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"FAKESYSTEMUI_SERVICE"'"

#define MATCH_DSME_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"DSME_SERVICE"'"

#define MATCH_MCE_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"MCE_SERVICE"'"

#if HAVE_LIBTIME

#define MATCH_CLOCKD_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"CLOCKD_SERVICE"'"

#define MATCH_CLOCKD_TIME_CHANGED\
  "type='signal'"\
  /*",sender='"CLOCKD_SERVICE"'"*/\
  ",interface='"CLOCKD_INTERFACE"'"\
  ",path='"CLOCKD_PATH"'"\
  ",member='"CLOCKD_TIME_CHANGED"'"

#endif /* HAVE_LIBTIME */

#define MATCH_DSME_SIGNALS \
  "type='signal'"\
  /*",sender='"DSME_SERVICE"'"*/\
  ",interface='"DSME_SIGNAL_IF"'"\
  ",path='"DSME_SIGNAL_PATH"'"

#define MATCH_DESKTOP_OWNER_CHANGED\
  "type='signal'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"DESKTOP_SERVICE"'"

static const char * const sessionbus_signals[] =
{
  MATCH_DESKTOP_OWNER_CHANGED,
  MATCH_STATUSAREA_CLOCK_OWNERCHANGED,
#if HAVE_LIBTIME
  MATCH_CLOCKD_OWNER_CHANGED,
#endif
  0
};

static const char * const systembus_signals[] =
{
  MATCH_SYSTEMUI_OWNER_CHANGED,
  MATCH_FAKESYSTEMUI_OWNER_CHANGED,

  MATCH_MCE_OWNER_CHANGED,

  MATCH_DSME_OWNER_CHANGED,
  MATCH_DSME_SIGNALS,

#if HAVE_LIBTIME
  MATCH_CLOCKD_OWNER_CHANGED,
  MATCH_CLOCKD_TIME_CHANGED,
#endif
  0
};

/* ========================================================================= *
 * Misc utilities
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * running_in_scratchbox  --  determine if we are being executed inside sbox
 * ------------------------------------------------------------------------- */

static
int
running_in_scratchbox(void)
{
  return access("/targets/links/scratchbox.config", F_OK) == 0;
}

/* ------------------------------------------------------------------------- *
 * tm_repr  --  struct tm to string
 * ------------------------------------------------------------------------- */

static
char *
tm_repr(const struct tm *tm, const char *tz, char *buff, size_t size)
{
  static const char * const wday[7] =
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  static const char * const mon[12] =
  {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  snprintf(buff, size,
           "%s %04d-%s-%02d %02d:%02d:%02d (%d/%d) TZ=%s",
           (tm->tm_wday < 0) ? "???" : wday[tm->tm_wday],
           (tm->tm_year < 0) ? -1 : (tm->tm_year + 1900),
           (tm->tm_mon  < 0) ? "???" : mon[tm->tm_mon],
           tm->tm_mday,
           tm->tm_hour,
           tm->tm_min,
           tm->tm_sec,
           tm->tm_yday,
           tm->tm_isdst,
           tz);
  return buff;
}

/* ------------------------------------------------------------------------- *
 * ticker_bump_time
 * ------------------------------------------------------------------------- */

static
time_t
ticker_bump_time(time_t base, time_t skip, time_t target)
{
  /* return base + skip * N that is larger than target */

  if( target < base )
  {
    time_t add = (base - target) % skip;
    return target + (add ? add : skip);
  }
  return target + skip - (target - base) % skip;
}

/* ------------------------------------------------------------------------- *
 * ticker_init_from_template
 * ------------------------------------------------------------------------- */

// QUARANTINE static
// QUARANTINE void
// QUARANTINE ticker_init_from_template(struct tm *trg, const struct tm *now, const struct tm *tpl)
// QUARANTINE {
// QUARANTINE #define X(v) trg->v = (tpl->v < 0) ? (now->v) : (tpl->v);
// QUARANTINE   X(tm_sec)
// QUARANTINE   X(tm_min)
// QUARANTINE   X(tm_hour)
// QUARANTINE   X(tm_mday)
// QUARANTINE   X(tm_mon)
// QUARANTINE   X(tm_year)
// QUARANTINE   X(tm_wday)
// QUARANTINE   X(tm_yday)
// QUARANTINE   X(tm_isdst)
// QUARANTINE #undef X
// QUARANTINE }

/* ========================================================================= *
 * Delayed Database Save
 * ========================================================================= */

static guint  server_queue_save_id = 0;

/* ------------------------------------------------------------------------- *
 * server_queue_save_cb  --  save for real after timeout
 * ------------------------------------------------------------------------- */

static
gboolean
server_queue_save_cb(gpointer data)
{
  queue_save();
  server_queue_save_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_queue_idle_cb  --  wait for idle before starting timeout
 * ------------------------------------------------------------------------- */

static
gboolean
server_queue_idle_cb(gpointer data)
{
  server_queue_save_id = g_timeout_add(SERVER_QUEUE_SAVE_DELAY_MSEC, server_queue_save_cb, 0);
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_queue_cancel_save  --  cancel save request
 * ------------------------------------------------------------------------- */

static
void
server_queue_cancel_save(void)
{
  if( server_queue_save_id != 0 )
  {
    g_source_remove(server_queue_save_id);
    server_queue_save_id = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_request_save  --  request db save after idle + timeout
 * ------------------------------------------------------------------------- */

static
void
server_queue_request_save(void)
{
  if( server_queue_save_id == 0 )
  {
    server_queue_save_id = g_idle_add(server_queue_idle_cb, 0);
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_forced_save  --  save db immediately
 * ------------------------------------------------------------------------- */

static
void
server_queue_forced_save(void)
{
  server_queue_cancel_save();
  server_queue_save_cb(0);
}

/* ========================================================================= *
 * Server State
 * ========================================================================= */

enum
{
  SF_CONNECTED    = 1 << 0, // we have internet connection
  SF_STARTUP      = 1 << 1, // alarmd is starting up

  SF_SYSTEMUI_UP  = 1 << 2, // system ui is currently up
  SF_SYSTEMUI_DN  = 1 << 3, // system ui has been down

  SF_CLOCKD_UP    = 1 << 4, // clockd is currently up
  SF_CLOCKD_DN    = 1 << 5, // clockd has been down

  SF_DSME_UP      = 1 << 6, // dsme is currently up
  SF_DSME_DN      = 1 << 7, // dsme has been down

  SF_STATUSBAR_UP = 1 << 8, // statusbar is currently up
  SF_STATUSBAR_DN = 1 << 9, // statusbar has been down

  SF_TZ_CHANGED   = 1 << 10, // timezone change from clockd
  SF_CLK_CHANGED  = 1 << 11, // clock change from clocd
  SF_CLK_MV_FORW  = 1 << 12, // clock moved forwards
  SF_CLK_MV_BACK  = 1 << 13, // clock moved backwards

  SF_FAKEUI_UP    = 1 << 14, // fake system ui is currently up
  SF_FAKEUI_DN    = 1 << 15, // fake system ui has been down

  SF_ACT_DEAD     = 1 << 16, // act dead mode active
  SF_DESKTOP_UP   = 1 << 17, // desktop boot finished

  SF_MCE_UP       = 1 << 18, // mce is currently up
  SF_MCE_DN       = 1 << 19, // mce has been down

  SF_CLK_BCAST    = 1 << 20, // clock change related actions done

  SF_SEND_POWERUP = 1 << 21, // act dead -> user from system ui

};

static unsigned server_state_prev = 0;
static unsigned server_state_real = 0;
static unsigned server_state_mask = 0;
static unsigned server_state_fake = 0;

static int server_icons_curr = 0; // alarms with statusbar icon flag
static int server_icons_prev = 0;

/* ------------------------------------------------------------------------- *
 * server_state_get
 * ------------------------------------------------------------------------- */

static
unsigned
server_state_get(void)
{
  return ((server_state_fake &  server_state_mask) |
          (server_state_real & ~server_state_mask));
}

/* ------------------------------------------------------------------------- *
 * server_state_clr
 * ------------------------------------------------------------------------- */

static
void
server_state_clr(unsigned clr)
{
  unsigned prev = server_state_get();

  server_state_real &= ~clr;
  server_state_prev = server_state_get();

  unsigned curr = server_state_get();

  if( prev != curr )
  {
    log_debug("CLR STATE: %08x -> %08x\n", prev, curr);
  }
}

/* ------------------------------------------------------------------------- *
 * server_state_set
 * ------------------------------------------------------------------------- */

static
void
server_state_set(unsigned clr, unsigned set)
{
  server_state_real &= ~clr;
  server_state_real |=  set;

  unsigned temp = server_state_get();
  if( server_state_prev != temp )
  {
    log_debug("SET STATE: %08x -> %08x\n", server_state_prev, temp);
    server_state_prev = temp;
    server_rethink_request();
  }
}

/* ========================================================================= *
 * Server Queue State
 * ========================================================================= */

typedef struct
{
  int    qs_alarms;     // active alarm dialogs
  time_t qs_desktop;    // nearest boot to desktop alarm
  time_t qs_actdead;    // nearest boot to acting dead alarm
  time_t qs_no_boot;    // nearest non booting alarm
} server_queuestate_t;

static server_queuestate_t server_queuestate_curr;
static server_queuestate_t server_queuestate_prev;

/* ------------------------------------------------------------------------- *
 * server_queuestate_reset
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_reset(void)
{
  // make sure all gaps are filled -> allows binary comparison
  memset(&server_queuestate_curr, 0, sizeof server_queuestate_curr);

  server_queuestate_curr.qs_alarms  = 0;
  server_queuestate_curr.qs_desktop = INT_MAX;
  server_queuestate_curr.qs_actdead = INT_MAX;
  server_queuestate_curr.qs_no_boot = INT_MAX;

  server_icons_curr = 0;
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_invalidate
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_invalidate(void)
{
  // set previous number of alarms to impossible
  // value -> force emitting queuestatus change signal
  server_queuestate_prev.qs_alarms = -1;
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_init
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_init(void)
{
  server_queuestate_reset();

  /* - - - - - - - - - - - - - - - - - - - *
   * reset queue state, make sure first
   * check yields difference
   * - - - - - - - - - - - - - - - - - - - */

  server_queuestate_prev = server_queuestate_curr;
  server_queuestate_invalidate();
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_sync
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_sync(void)
{
  if( server_state_get() & SF_STATUSBAR_UP )
  {
    if( server_icons_prev != server_icons_curr )
    {
      if( server_icons_curr > 0 )
      {
        if( server_icons_prev <= 0 )
        {
          statusbar_alarm_show(server_session_bus);
        }
      }
      else
      {
        statusbar_alarm_hide(server_session_bus);
      }
      server_icons_prev = server_icons_curr;
    }
  }

  if( memcmp(&server_queuestate_prev, &server_queuestate_curr,
             sizeof server_queuestate_curr) )
  {
    time_t t = ticker_get_time();

    auto int timeto(time_t d);
    auto int timeto(time_t d)
    {
      return (d < t) ? 0 : (d < INT_MAX) ? (int)(d-t) : 9999;
    }

    log_debug("QSTATE: active=%d, desktop=%d, act dead=%d, no boot=%d\n",
              server_queuestate_curr.qs_alarms,
              timeto(server_queuestate_curr.qs_desktop),
              timeto(server_queuestate_curr.qs_actdead),
              timeto(server_queuestate_curr.qs_no_boot));

    // FIXME: find proper place for this
    {
      dbus_int32_t c = server_queuestate_curr.qs_alarms;
      dbus_int32_t d = server_queuestate_curr.qs_desktop;
      dbus_int32_t a = server_queuestate_curr.qs_actdead;
      dbus_int32_t n = server_queuestate_curr.qs_no_boot;

      /* - - - - - - - - - - - - - - - - - - - *
       * send signal to session bus
       * - - - - - - - - - - - - - - - - - - - */

      dbusif_signal_send(server_session_bus,
                         ALARMD_PATH,
                         ALARMD_INTERFACE,
                         ALARMD_QUEUE_STATUS_IND,
                         DBUS_TYPE_INT32, &c,
                         DBUS_TYPE_INT32, &d,
                         DBUS_TYPE_INT32, &a,
                         DBUS_TYPE_INT32, &n,
                         DBUS_TYPE_INVALID);

      /* - - - - - - - - - - - - - - - - - - - *
       * send also to system bus so that dsme
       * can keep off the session bus
       * - - - - - - - - - - - - - - - - - - - */

      dbusif_signal_send(server_system_bus,
                         ALARMD_PATH,
                         ALARMD_INTERFACE,
                         ALARMD_QUEUE_STATUS_IND,
                         DBUS_TYPE_INT32, &c,
                         DBUS_TYPE_INT32, &d,
                         DBUS_TYPE_INT32, &a,
                         DBUS_TYPE_INT32, &n,
                         DBUS_TYPE_INVALID);

    }
    server_queuestate_prev = server_queuestate_curr;
  }
}

/* ========================================================================= *
 * Server Time State
 * ========================================================================= */

// QUARANTINE typedef struct
// QUARANTINE {
// QUARANTINE   int dst;
// QUARANTINE   char tz[128];
// QUARANTINE
// QUARANTINE } server_timestate_t;
// QUARANTINE
// QUARANTINE static server_timestate_t server_timestate_prev;
// QUARANTINE static server_timestate_t server_timestate_curr;

static char server_tz_prev[128];  // current and previous timezone
static char server_tz_curr[128];

static int  server_dst_prev = -1;
static int  server_dst_curr = -1;

/* ------------------------------------------------------------------------- *
 * server_timestate_read
 * ------------------------------------------------------------------------- */

static
void
server_timestate_read(void)
{
  struct tm tm;
  ticker_get_timezone(server_tz_curr, sizeof server_tz_curr);
  ticker_get_local(&tm);
  server_dst_curr = tm.tm_isdst;
}

/* ------------------------------------------------------------------------- *
 * server_timestate_sync
 * ------------------------------------------------------------------------- */

static
void
server_timestate_sync(void)
{
  strcpy(server_tz_prev, server_tz_curr);
  server_dst_prev = server_dst_curr;
}

/* ------------------------------------------------------------------------- *
 * server_timestate_changed
 * ------------------------------------------------------------------------- */

static
int
server_timestate_changed(void)
{
  return (strcmp(server_tz_prev, server_tz_curr) ||
          server_dst_prev != server_dst_curr);
}

/* ------------------------------------------------------------------------- *
 * server_timestate_init
 * ------------------------------------------------------------------------- */

static
void
server_timestate_init(void)
{
  ticker_get_synced();
  server_timestate_read();
  server_timestate_sync();

  log_debug("TIMEZONE: '%s'\n", server_tz_curr);
}

/* ========================================================================= *
 * Server State
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_state_init
 * ------------------------------------------------------------------------- */

static
void
server_state_init(void)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * starting up ...
   * - - - - - - - - - - - - - - - - - - - */

  server_state_real |= SF_STARTUP;

  /* - - - - - - - - - - - - - - - - - - - *
   * make synchronization happen
   * - - - - - - - - - - - - - - - - - - - */

  server_state_real |= SF_SYSTEMUI_DN;
  server_state_real |= SF_CLOCKD_DN;
  server_state_real |= SF_DSME_DN;
  server_state_real |= SF_MCE_DN;
  server_state_real |= SF_STATUSBAR_DN;
  server_state_real |= SF_FAKEUI_DN;

  server_icons_prev  = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * check existence of peer services
   * - - - - - - - - - - - - - - - - - - - */

  if( dbusif_check_name_owner(server_system_bus, SYSTEMUI_SERVICE) == 0 )
  {
    server_state_real |= SF_SYSTEMUI_UP;
  }

  if( dbusif_check_name_owner(server_system_bus, FAKESYSTEMUI_SERVICE) == 0 )
  {
    server_state_real |= SF_FAKEUI_UP;
  }

  if( dbusif_check_name_owner(server_session_bus, CLOCKD_SERVICE) == 0 ||
      dbusif_check_name_owner(server_system_bus, CLOCKD_SERVICE) == 0 )
  {
    server_state_real |= SF_CLOCKD_UP;
    ticker_use_libtime(TRUE);
  }
  else
  {
    ticker_use_libtime(FALSE);
  }
  server_timestate_init();

  if( dbusif_check_name_owner(server_system_bus, DSME_SERVICE) == 0 )
  {
    server_state_real |= SF_DSME_UP;
  }
  if( dbusif_check_name_owner(server_system_bus, MCE_SERVICE) == 0 )
  {
    server_state_real |= SF_MCE_UP;
  }

  if( dbusif_check_name_owner(server_session_bus, STATUSAREA_CLOCK_SERVICE) == 0 )
  {
    server_state_real |= SF_STATUSBAR_UP;

// QUARANTINE     log_debug("statusarea clock plugin detected -> LIMBO disabled\n");
// QUARANTINE     server_state_real |= SF_DESKTOP_UP;
  }

  if( dbusif_check_name_owner(server_session_bus, DESKTOP_SERVICE) == 0 )
  {
    log_debug("desktop ready detected -> LIMBO disabled\n");
    server_state_real |= SF_DESKTOP_UP;
  }

  if( access("/tmp/ACT_DEAD", F_OK) == 0 )
  {
    log_debug("not in USER state -> LIMBO enabled\n");
    server_state_real |= SF_ACT_DEAD;
  }

  server_state_clr(0);
}

/* ------------------------------------------------------------------------- *
 * server_state_sync
 * ------------------------------------------------------------------------- */

static
void
server_state_sync(void)
{
  unsigned flgs = server_state_get();

  unsigned mask;

  /* - - - - - - - - - - - - - - - - - - - *
   * clockd
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_CLOCKD_DN | SF_CLOCKD_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_CLOCKD_DN);

    // sync with emergent clockd
    ticker_get_synced();

    server_timestate_read();
    if( server_timestate_changed() )
    {
      server_state_set(0, SF_TZ_CHANGED);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * dsme
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_DSME_DN | SF_DSME_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_DSME_DN);

    // make sure status broadcast is made
    server_queuestate_invalidate();
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * mce
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_MCE_DN | SF_MCE_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_MCE_DN);

    // make sure status broadcast is made
    server_queuestate_invalidate();
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * statusbar
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_STATUSBAR_DN | SF_STATUSBAR_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_STATUSBAR_DN);

    // make sure status broadcast is made
    server_icons_prev = -1;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * systemui
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_FAKEUI_DN | SF_FAKEUI_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_FAKEUI_DN);

    // systemui related actions handled directly
    // in alarm event state transition logic
  }

  mask = SF_SYSTEMUI_DN | SF_SYSTEMUI_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_SYSTEMUI_DN);

    // systemui related actions handled directly
    // in alarm event state transition logic
  }

  // done
  server_state_prev = server_state_get();
}

/* ------------------------------------------------------------------------- *
 * server_state_get_systemui_service
 * ------------------------------------------------------------------------- */

static
const char *
server_state_get_systemui_service(void)
{
  unsigned flags = server_state_get();

  if( flags & SF_FAKEUI_UP )
  {
    return FAKESYSTEMUI_SERVICE;
  }
  if( flags & SF_SYSTEMUI_UP )
  {
    return SYSTEMUI_SERVICE;
  }
  return NULL;
}

/* ========================================================================= *
 * Alarm Action Functionality
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_action_get_type  --  get action type mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_action_get_type(alarm_action_t *action)
{
  return action ? (action->flags & ALARM_ACTION_TYPE_MASK) : 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_get_when  -- get action when mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_action_get_when(alarm_action_t *action)
{
  return action ? (action->flags & ALARM_ACTION_WHEN_MASK) : 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_snooze  --  execute snooze action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_snooze(alarm_event_t *event, alarm_action_t *action)
{
  queue_set_event_state(event, ALARM_STATE_SNOOZED);
  return 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_disable  --  execute disable action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_disable(alarm_event_t *event, alarm_action_t *action)
{
  /* The event comes invisible to alarmd, no state transition
   * required. The client application needs to:
   * - delete the event via alarmd_event_del()
   * - re-enable the event via alarmd_event_update()
   *
   * In the latter case the event will re-enter the
   * state machine via ALARM_STATE_NEW state.
   */
  log_debug("DISABLING DUE TO ACTION: cookie=%d\n", (int)event->cookie);
  event->flags |= ALARM_EVENT_DISABLED;
  return 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_desktop  --  execute boot to desktop action
 * ------------------------------------------------------------------------- */

#if ALARMD_ACTION_BOOTFLAGS
static
int
server_action_do_desktop(alarm_event_t *event, alarm_action_t *action)
{
#ifdef DEAD_CODE
  dsme_req_powerup(server_system_bus);
#endif
  return 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_action_do_actdead  --  execute boot to acting dead action
 * ------------------------------------------------------------------------- */

#if ALARMD_ACTION_BOOTFLAGS
static
int
server_action_do_actdead(alarm_event_t *event, alarm_action_t *action)
{
  /* nothing to do */
  return 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_action_do_exec  --  execute run command line action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_exec(alarm_event_t *event, alarm_action_t *action)
{
  static const char tag[] = "[COOKIE]";

  int         err = -1;
  const char *cmd = action->exec_command;
  const char *hit = 0;
  char       *tmp = 0;

  if( xisempty(cmd) )
  {
    goto cleanup;
  }

  if( action->flags & ALARM_ACTION_EXEC_ADD_COOKIE )
  {
    if( (hit = strstr(cmd, tag)) != 0 )
    {
      asprintf(&tmp, "%*s%d%s",
               (int)(hit-cmd), cmd,
               (int)event->cookie,
               hit + sizeof tag - 1);
    }
    else
    {
      asprintf(&tmp, "%s %d", cmd, (int)event->cookie);
    }
    cmd = tmp;
  }

  if( xisempty(cmd) )
  {
    goto cleanup;
  }

  log_debug("EXEC: %s\n", cmd);
  ipc_exec_run_command(cmd);
  err = 0;

  cleanup:

  free(tmp);
  return 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_dbus  --  execute send dbus message action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_dbus(alarm_event_t *event, alarm_action_t *action)
{
  int          err = -1;
  DBusMessage *msg = 0;

  log_debug("DBUS: service   = '%s'\n", action->dbus_service);
  log_debug("DBUS: path      = '%s'\n", action->dbus_path);
  log_debug("DBUS: interface = '%s'\n", action->dbus_interface);
  log_debug("DBUS: name      = '%s'\n", action->dbus_name);

  if( !xisempty(action->dbus_service) )
  {
    log_debug("DBUS: is method\n");
    msg = dbus_message_new_method_call(action->dbus_service,
                                       action->dbus_path,
                                       action->dbus_interface,
                                       action->dbus_name);

    if( action->flags & ALARM_ACTION_DBUS_USE_ACTIVATION )
    {
      log_debug("DBUS: is auto start\n");
      dbus_message_set_auto_start(msg, TRUE);
    }
    dbus_message_set_no_reply(msg, TRUE);
  }
  else
  {
    log_debug("DBUS: is signal\n");
    msg = dbus_message_new_signal(action->dbus_path,
                                  action->dbus_interface,
                                  action->dbus_name);
  }

  if( msg != 0 )
  {
    DBusConnection *conn = server_session_bus;

    if( !xisempty(action->dbus_args) )
    {
      log_debug("DBUS: has user args\n");
      serialize_unpack_to_mesg(action->dbus_args, msg);
    }

    if( action->flags & ALARM_ACTION_DBUS_ADD_COOKIE )
    {
      log_debug("DBUS: appending cookie %d\n", (int)event->cookie);

      dbus_int32_t cookie = event->cookie;
      dbus_message_append_args(msg,
                           DBUS_TYPE_INT32, &cookie,
                           DBUS_TYPE_INVALID);
    }

    if( action->flags & ALARM_ACTION_DBUS_USE_SYSTEMBUS )
    {
      log_debug("DBUS: using system bus\n");
      conn = server_system_bus;
    }

    if( conn != 0 && dbus_connection_send(conn, msg, 0) )
    {
      log_debug("DBUS: send ok\n");
      err = 0;
    }
  }

  log_debug("DBUS: error=%d\n", err);

  if( msg != 0 )
  {
    dbus_message_unref(msg);
  }
  return err;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_all  --  execute all configured actions
 * ------------------------------------------------------------------------- */

static
void
server_action_do_all(alarm_event_t *event, alarm_action_t *action)
{
  if( event && action )
  {
    unsigned flags = server_action_get_type(action);

    if( flags & ALARM_ACTION_TYPE_SNOOZE )
    {
      log_debug("ACTION: SNOOZE\n");
      server_action_do_snooze(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_DISABLE )
    {
      log_debug("ACTION: DISABLE\n");
      server_action_do_disable(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_DBUS )
    {
      log_debug("ACTION: DBUS\n");
      server_action_do_dbus(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_EXEC )
    {
      log_debug("ACTION: EXEC: %s\n", action->exec_command);
      server_action_do_exec(event, action);
    }
#if ALARMD_ACTION_BOOTFLAGS
    if( flags & ALARM_ACTION_TYPE_DESKTOP )
    {
      log_debug("ACTION: DESKTOP\n");
      server_action_do_desktop(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_ACTDEAD )
    {
      log_debug("ACTION: ACTDEAD\n");
      server_action_do_actdead(event, action);
    }
#endif
  }
}

/* ========================================================================= *
 * Server Event Handling Functions
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_event_do_state_actions  --  execute actions that match given when mask
 * ------------------------------------------------------------------------- */

static
void
server_event_do_state_actions(alarm_event_t *eve, unsigned when)
{
  if( eve && when )
  {
    for( int i = 0; i < eve->action_cnt; ++i )
    {
      alarm_action_t *act = &eve->action_tab[i];
      if( server_action_get_when(act) & when )
      {
        server_action_do_all(eve, act);
      }
    }
  }
}

/* ------------------------------------------------------------------------- *
 * server_event_do_response_actions  --  execute actions for user response
 * ------------------------------------------------------------------------- */

static
void
server_event_do_response_actions(alarm_event_t *eve)
{
  if( 0 <= eve->response && eve->response < eve->action_cnt )
  {
    alarm_action_t *act = &eve->action_tab[eve->response];

    if( server_action_get_when(act) & ALARM_ACTION_WHEN_RESPONDED )
    {
      server_action_do_all(eve, act);
    }
  }
}

/* ------------------------------------------------------------------------- *
 * server_event_get_tz  --  return timezone from event or current default
 * ------------------------------------------------------------------------- */

static
const char *
server_event_get_tz(alarm_event_t *self)
{
  if( !xisempty(self->alarm_tz) )
  {
    return self->alarm_tz;
  }
  return server_tz_curr;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_boot_mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_event_get_boot_mask(alarm_event_t *eve)
{
  unsigned acc = eve->flags & (ALARM_EVENT_ACTDEAD | ALARM_EVENT_BOOT);

#if ALARMD_ACTION_BOOTFLAGS
  for( int i = 0; i < eve->action_cnt; ++i )
  {
    alarm_action_t *act = &eve->action_tab[i];

    if( act->flags & ALARM_ACTION_TYPE_DESKTOP )
    {
      acc |= ALARM_EVENT_BOOT;
    }
    if( act->flags & ALARM_ACTION_TYPE_ACTDEAD )
    {
      acc |= ALARM_EVENT_ACTDEAD;
    }
  }
#endif

  return acc;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_snooze
 * ------------------------------------------------------------------------- */

static
time_t
server_event_get_snooze(alarm_event_t *eve)
{
  time_t snooze = eve->snooze_secs;

  if( snooze <= 0 )
  {
    snooze = queue_get_snooze();
  }
  if( snooze < 10 )
  {
    snooze = 10;
  }
  return snooze;
}

/* ------------------------------------------------------------------------- *
 * server_event_is_snoozed
 * ------------------------------------------------------------------------- */

static
int
server_event_is_snoozed(alarm_event_t *eve)
{
  return (eve->snooze_total != 0);
}

/* ------------------------------------------------------------------------- *
 * server_event_get_buttons
 * ------------------------------------------------------------------------- */

static
int
server_event_get_buttons(alarm_event_t *self, int *vec, int size)
{
  int cnt = 0;

  for( int i = 0; i < self->action_cnt; ++i )
  {
    alarm_action_t *act = &self->action_tab[i];

    if( !xisempty(act->label) && (act->flags & ALARM_ACTION_WHEN_RESPONDED) )
    {
      if( cnt < size )
      {
        vec[cnt] = i;
      }
      ++cnt;
    }
  }
  return cnt;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_next_trigger
 * ------------------------------------------------------------------------- */

static
time_t
server_event_get_next_trigger(time_t t0, time_t t1, alarm_event_t *self)
{
  const char *tz = server_event_get_tz(self);

  /* was alarm time specified using broken down time ? */
  if( t1 <= 0 )
  {
    int           fqt = 1;
    struct tm     tpl = self->alarm_tm;
    struct tm     now;
    alarm_recur_t rec;

    ticker_break_tm(t0, &now, tz);

    alarm_recur_ctor(&rec);

    if( tpl.tm_sec  < 0 ) tpl.tm_sec = 0;
    if( tpl.tm_min  < 0 ) fqt=0; else rec.mask_min  = 1ull << tpl.tm_min;
    if( tpl.tm_hour < 0 ) fqt=0; else rec.mask_hour = 1u   << tpl.tm_hour;

    if( tpl.tm_wday < 0 ) {}     else rec.mask_wday = 1u   << tpl.tm_wday;
    if( tpl.tm_mday < 0 ) fqt=0; else rec.mask_mday = 1u   << tpl.tm_mday;
    if( tpl.tm_mon  < 0 ) fqt=0; else rec.mask_mon  = 1u   << tpl.tm_mon;
    if( tpl.tm_year < 0 ) fqt=0;

    if( fqt )
    {
      t1 = ticker_mktime(&tpl, tz);
// QUARANTINE       log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
    }
    else
    {
      t1 = alarm_recur_align(&rec, &now, tz);
// QUARANTINE       log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
    }

    alarm_recur_dtor(&rec);
  }
// QUARANTINE   else
// QUARANTINE   {
// QUARANTINE     log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
// QUARANTINE   }

  /* for recurring events with recurrence masks, the
   * above calculated trigger time is low bound and
   * we need to get to the next unmasked point in time */

  if( alarm_event_is_recurring(self) )
  {
    if( self->recur_secs > 0 )
    {
      // simple recurrence
      if( t1 <= t0 )
      {
        t1 = ticker_bump_time(t1, self->recur_secs, t0);
      }
    }
    else
    {
      // recurrence masks
      time_t next = INT_MAX;
      struct tm use;

      if( t1 < t0 )
      {
        t1 = t0;
// QUARANTINE         log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
      }

      ticker_break_tm(t1, &use, tz);

      for( size_t i = 0; i < self->recurrence_cnt; ++i )
      {
        alarm_recur_t *rec = &self->recurrence_tab[i];
        struct tm tmp = use;
        time_t    trg = alarm_recur_align(rec, &tmp, tz);

        if( t1 < trg && trg < next )
        {
          next = trg;
// QUARANTINE           log_debug_L("T1 ? %d: %s", (int)trg, ctime(&trg));
        }
      }

      if( t1 < next && next < INT_MAX )
      {
        t1 = next;
// QUARANTINE         log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
      }
    }
  }

  if( t1 < t0 )
  {
    t1 = -1;
    log_debug_L("T1 = %d: %s", (int)t1, "REJECTED");
  }
// QUARANTINE   else
// QUARANTINE   {
// QUARANTINE     log_debug_L("T1 = %d: %s", (int)t1, ctime(&t1));
// QUARANTINE   }

  return t1;
}

/* ------------------------------------------------------------------------- *
 * server_event_set_trigger
 * ------------------------------------------------------------------------- */

static
int
server_event_set_trigger(alarm_event_t *self)
{
  time_t t0 = ticker_get_time();
  time_t t1 = server_event_get_next_trigger(t0, self->alarm_time, self);

  self->trigger = t1;
  log_debug("!!! SET TRIGGER -> %+d secs @ %s\n", (int)(t1 - t0), ctime(&t1));

  return (t1 >= t0) ? 0 : -1;
}

/* ------------------------------------------------------------------------- *
 * server_clock_source_is_stable
 * ------------------------------------------------------------------------- */

enum
{
  CLK_JITTER  = 2, /* If alarmd notices that system time vs. monotonic
                    * time changes more than CLK_JITTER seconds ... */

  CLK_STABLE  = 2, /* ... it will delay alarm event state transition
                    * evaluation for CLK_STABLE seconds. */

  CLK_RESCHED = 5, /* If detected change is larger than CLK_RESCHED secs,
                    * the trigger times for certain alarms will be
                    * re-evaluated */
};

static time_t delta_back = 0;
static time_t delta_forw = 0;

static time_t server_clock_back_delta(void)
{
  time_t t = delta_back; delta_back = 0; return t;
}
static time_t server_clock_forw_delta(void)
{
  time_t t = delta_forw; delta_forw = 0; return t;
}

static
gboolean
server_clock_source_is_stable(void)
{
  static int    initialized = 0;
  static time_t delta_sched = 0;
  static time_t delta_clock = 0;
  static time_t delay_until = 0;

  time_t real_time  = ticker_get_time();
  time_t mono_time  = ticker_get_monotonic();
  time_t delta      = real_time - mono_time;
  time_t delta_diff = 0;
  int    detected   = 0;

  if( !initialized )
  {
    log_debug("INITIALIZING CLOCK SOURCE ...\n");
    initialized = 1;
    delta_clock = delta;
    delta_sched = delta;
    delay_until = mono_time;
  }

  if( server_state_get() & SF_CLK_CHANGED )
  {
    log_debug("CLOCK CHANGE REPORTED\n");
    server_state_clr(SF_CLK_CHANGED);
    detected = 1;
  }

  delta_diff = delta - delta_clock;

  if( delta_diff < -CLK_JITTER || +CLK_JITTER < delta_diff )
  {
    log_debug("CLOCK CHANGE DETECTED: %+d seconds\n", delta_diff);
    detected = 1;
  }

  if( detected != 0 )
  {
    delta_clock = delta;
    delay_until  = mono_time + CLK_STABLE;
  }

  if( mono_time < delay_until )
  {
    log_debug("WAITING CLOCK SOURCE TO STABILIZE ...\n");
    return FALSE;
  }

  delta_diff = delta_clock - delta_sched;

  if( delta_diff > +CLK_RESCHED )
  {
    log_debug("overall change: %+d seconds\n", (int)delta_diff);
    delta_sched = delta_clock;
    delta_forw += delta_diff;
    server_state_set(0, SF_CLK_MV_FORW);
  }
  else if( delta_diff < -CLK_RESCHED )
  {
    log_debug("overall change: %+d seconds\n", (int)delta_diff);
    delta_sched = delta_clock;
    delta_back += delta_diff;
    server_state_set(0, SF_CLK_MV_BACK);
  }

  return TRUE;
}

/* ========================================================================= *
 * Server Wakeup for alarm
 * ========================================================================= */

static time_t server_wakeup_time = INT_MAX;
static guint  server_wakeup_id   = 0;

/* ------------------------------------------------------------------------- *
 * server_wakeup_start_cb
 * ------------------------------------------------------------------------- */

static
gboolean
server_wakeup_start_cb(gpointer data)
{
  server_wakeup_time = INT_MAX;
  server_wakeup_id   = 0;
  server_rethink_request();
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_cancel_wakeup
 * ------------------------------------------------------------------------- */

static
void
server_cancel_wakeup(void)
{
  if( server_wakeup_id != 0 )
  {
    g_source_remove(server_wakeup_id);
    server_wakeup_id = 0;
  }
  server_wakeup_time = INT_MAX;
}

/* ------------------------------------------------------------------------- *
 * server_request_wakeup
 * ------------------------------------------------------------------------- */

static
void
server_request_wakeup(time_t tmo)
{
  time_t now = ticker_get_time();
  time_t top = now + 14 * 24 * 60 * 60; // two weeks ahead

  {
    struct tm tm; char tmp[128];
    const char *tz = server_tz_curr;
    ticker_break_tm(tmo, &tm, tz);
    tm_repr(&tm, tz, tmp, sizeof tmp);
    log_debug("REQ WAKEUP: %s\n", tmp);
  }

  if( tmo > top )
  {
    tmo = top;
  }

  if( server_wakeup_time > tmo )
  {
    if( server_wakeup_id != 0 )
    {
      g_source_remove(server_wakeup_id);
      server_wakeup_id = 0;
    }

    server_wakeup_time = tmo;

    {
      const char *tz = server_tz_curr;
      struct tm tm; char tmp[128];
      ticker_break_tm(tmo, &tm, tz);
      tm_repr(&tm, tz, tmp, sizeof tmp);
      log_debug("SET WAKEUP: %s (%+d)\n", tmp, (tmo-now));
    }

    if( tmo <= now )
    {
      server_wakeup_id = g_idle_add(server_wakeup_start_cb, 0);
    }
    else
    {
      tmo -= now;

      //TODO: fix when g_timeout_add_seconds() is available
      //server_wakeup_id = g_timeout_add_seconds(tmo, server_wakeup_start_cb, 0);
      server_wakeup_id = g_timeout_add(tmo*1000, server_wakeup_start_cb, 0);
    }
  }
}

/* ========================================================================= *
 * Rething states of queued events
 * ========================================================================= */

static int    server_rethink_req = 0;
static guint  server_rethink_id  = 0;

static time_t server_rethink_time = 0;

static inline void time_filt(time_t *low, time_t add)
{
  if( *low > add ) *low = add;
}

/* ------------------------------------------------------------------------- *
 * server_handle_systemui_ack
 * ------------------------------------------------------------------------- */

static
int
server_handle_systemui_ack(cookie_t *vec, int cnt)
{
  int err = 0;

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    if( eve != 0 )
    {
      queue_set_event_state(eve, ALARM_STATE_SYSUI_ACK);
    }
  }
  server_rethink_request();

  return err;
}

/* ------------------------------------------------------------------------- *
 * server_systemui_ack_cb
 * ------------------------------------------------------------------------- */

static
void
server_systemui_ack_cb(dbus_int32_t *vec, int cnt)
{
  server_handle_systemui_ack((cookie_t*)vec, cnt);
}

/* ------------------------------------------------------------------------- *
 * server_handle_systemui_rsp
 * ------------------------------------------------------------------------- */

static
int
server_handle_systemui_rsp(cookie_t cookie, int button)
{
  int err = 0;

  alarm_event_t *eve = queue_get_event(cookie);

  log_debug("rsp %ld -> %p (button=%d)\n", cookie, eve, button);

  if( eve != 0 )
  {
    queue_set_event_state(eve, ALARM_STATE_SYSUI_RSP);
    eve->response = button;
  }

  server_rethink_request();

  return err;
}

/* ------------------------------------------------------------------------- *
 * server_rethink_new
 * ------------------------------------------------------------------------- */

static
void
server_rethink_new(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;

  vec = queue_query_by_state(&cnt, ALARM_STATE_NEW);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

#if ENABLE_LOGGING >= 3
    {
      time_t    now = server_rethink_time;
      time_t    tot = eve->trigger - eve->alarm_time;
      time_t    rec = eve->recur_secs ?: 1;
      const char *tz = server_event_get_tz(eve);
      struct tm tm;
      char trg[128];
      ticker_break_tm(eve->trigger, &tm, tz);
      tm_repr(&tm, tz, trg, sizeof trg);

      log_debug("NEW: %s at: %+d, %+d %% %d = %+d\n",
                trg,
                (int)eve->trigger - now,
                (int)tot,(int)rec,(int)(tot%rec));
    }
#endif
    // required internet connection not available?
    if( eve->flags & ALARM_EVENT_CONNECTED )
    {
      if( !(server_state_get() & SF_CONNECTED) )
      {
        queue_set_event_state(eve, ALARM_STATE_WAITCONN);
        continue;
      }
    }

    queue_set_event_state(eve, ALARM_STATE_QUEUED);
    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_QUEUED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_waitconn
 * ------------------------------------------------------------------------- */

static
void
server_rethink_waitconn(void)
{
  if( server_state_get() & SF_CONNECTED )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_WAITCONN);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_NEW);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_queued
 * ------------------------------------------------------------------------- */

static
void
server_rethink_queued(void)
{
  time_t    now = server_rethink_time;
  time_t    tsw = INT_MAX;
  time_t    thw = INT_MAX;
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    // required internet connection not available?
    if( eve->flags & ALARM_EVENT_CONNECTED )
    {
      if( !(server_state_get() & SF_CONNECTED) )
      {
        queue_set_event_state(eve, ALARM_STATE_WAITCONN);
        continue;
      }
    }

    {
      const char *tz = server_event_get_tz(eve);
      struct tm tm;
      char trg[128];
      ticker_break_tm(eve->trigger, &tm, tz);
      tm_repr(&tm, tz, trg, sizeof trg);
      log_debug("Q: %s, at %+d\n", trg, (int)(eve->trigger - now));
    }

    // in future -> just update wakeup time values
    if( eve->trigger > now )
    {
      unsigned boot = server_event_get_boot_mask(eve);

      if( boot & ALARM_EVENT_BOOT )
      {
        time_filt(&server_queuestate_curr.qs_desktop, eve->trigger);
      }
      else if( boot & ALARM_EVENT_ACTDEAD )
      {
        time_filt(&server_queuestate_curr.qs_actdead, eve->trigger);
      }
      else
      {
        time_filt(&server_queuestate_curr.qs_no_boot, eve->trigger);
      }

      if( eve->flags & ALARM_EVENT_SHOW_ICON )
      {
        server_icons_curr += 1;
      }
      continue;
    }

    // missed for some reason, power off for example
    if( (now - eve->trigger) > SERVER_MISSED_LIMIT )
    {
      queue_set_event_state(eve, ALARM_STATE_MISSED);
      continue;
    }

    // trigger it
    queue_set_event_state(eve, ALARM_STATE_LIMBO);
  }

  // determinetimeout values
  time_filt(&thw, server_queuestate_curr.qs_desktop);
  time_filt(&thw, server_queuestate_curr.qs_actdead);

  time_filt(&tsw, thw);
  time_filt(&tsw, server_queuestate_curr.qs_no_boot);

  // set software timeout
  if( tsw < INT_MAX )
  {
    server_request_wakeup(tsw);
  }

  // set hardware timeout
  if( thw < INT_MAX )
  {
    struct tm tm;

    time_t lim = now + ALARM_INTERRUPT_LIMIT_SECS;
    time_t trg = thw - POWERUP_COMPENSATION_SECS;

    if( trg < lim ) trg = lim;
    gmtime_r(&trg, &tm);
    hwrtc_set_alarm(&tm, 1);
  }

  // cleanup
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_missed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_missed(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;

  vec = queue_query_by_state(&cnt, ALARM_STATE_MISSED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    /* - - - - - - - - - - - - - - - - - - - *
     * handle actions bound to missed alarms
     * - - - - - - - - - - - - - - - - - - - */

    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DELAYED);

    /* - - - - - - - - - - - - - - - - - - - *
     * next state for missed alarms depends
     * on event configuration bits
     * - - - - - - - - - - - - - - - - - - - */

    if( eve->flags & ALARM_EVENT_RUN_DELAYED )
    {
      queue_set_event_state(eve, ALARM_STATE_LIMBO);
      continue;
    }
    if( eve->flags & ALARM_EVENT_POSTPONE_DELAYED )
    {
      queue_set_event_state(eve, ALARM_STATE_POSTPONED);
      continue;
    }
    if( eve->flags & ALARM_EVENT_DISABLE_DELAYED )
    {
      log_debug("DISABLING MISSED ALARM: cookie=%d\n", (int)eve->cookie);
      // the event will come invisible to alarmd state machine
      // once the ALARM_EVENT_DISABLED flag is set
      // -> no state transfer required
      eve->flags |= ALARM_EVENT_DISABLED;
      server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DISABLED);
      continue;
    }
    //queue_set_event_state(eve, ALARM_STATE_DELETED);
    queue_set_event_state(eve, ALARM_STATE_SERVED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_postponed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_postponed(void)
{
  time_t    day = 24 * 60 * 60;
  time_t    now = server_rethink_time;
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_POSTPONED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    time_t snooze = server_event_get_snooze(eve);

    time_t trg = eve->trigger;

    /* trigger anyway if less than one day late */
    if( now - trg - snooze < day )
    {
      queue_set_event_state(eve, ALARM_STATE_LIMBO);
      continue;
    }

    /* otherwise autosnooze by one day */
    time_t add = now + day - eve->trigger;
    time_t pad = add % snooze;

    if( pad != 0 )
    {
      add = add - pad + snooze;
    }

    trg += add;

    queue_set_event_trigger(eve, trg);
    queue_set_event_state(eve, ALARM_STATE_NEW);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_limbo
 * ------------------------------------------------------------------------- */

static
void
server_rethink_limbo(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;

  if( server_state_get() & SF_ACT_DEAD )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * if we are in acting dead, only alarms
     * with ALARM_EVENT_ACTDEAD will pass
     * through the limbo
     * - - - - - - - - - - - - - - - - - - - */

    vec = queue_query_by_state(&cnt, ALARM_STATE_LIMBO);
    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);

      if( eve->flags & ALARM_EVENT_ACTDEAD )
      {
        queue_set_event_state(eve, ALARM_STATE_TRIGGERED);
      }
    }
  }
  else if( server_state_get() & SF_DESKTOP_UP )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * if we are in user mode and desktop has
     * been started up, all events will pass
     * through from the limbo
     * - - - - - - - - - - - - - - - - - - - */

    vec = queue_query_by_state(&cnt, ALARM_STATE_LIMBO);
    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_TRIGGERED);
    }
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_triggered
 * ------------------------------------------------------------------------- */

static
void
server_rethink_triggered(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_TRIGGERED);
  int       req = 0;

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_TRIGGERED);

    {
      struct tm tm;
      char now[128];
      char trg[128];
      const char *tz = server_event_get_tz(eve);

      ticker_break_tm(server_rethink_time, &tm, tz);
      tm_repr(&tm, tz, now, sizeof now);

      ticker_break_tm(eve->trigger, &tm, tz);
      tm_repr(&tm, tz, trg, sizeof trg);

      log_debug("TRIGGERED: %s -- %s\n", now, trg);
    }

    if( server_event_get_buttons(eve, 0,0) )
    {
      // transfer control to system ui
      vec[req++] = eve->cookie;
      queue_set_event_state(eve, ALARM_STATE_WAITSYSUI);
    }
    else if( queue_get_event_state(eve) == ALARM_STATE_SNOOZED )
    {
      // if snoozed, stay snoozed
    }
    else
    {
      // otherwise mark served
      queue_set_event_state(eve, ALARM_STATE_SERVED);
    }
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_waitsysui
 * ------------------------------------------------------------------------- */

static
void
server_rethink_waitsysui(void)
{
  if( server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_WAITSYSUI);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_SYSUI_REQ);
    }

    if( cnt != 0 )
    {
      systemui_add_dialog(server_system_bus, vec, cnt);
    }
    free(vec);
  }

}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_req
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_req(void)
{
  /* Note: transition ALARM_STATE_SYSUI_REQ -> ALARM_STATE_SYSUI_ACK
   *       is done via callback server_handle_systemui_ack() */

  if( !server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_REQ);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_WAITSYSUI);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_ack
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_ack(void)
{
  /* Note: transition ALARM_STATE_SYSUI_ACK -> ALARM_STATE_SYSUI_RSP
   *       is done via callback server_handle_systemui_rsp() */

  if( !server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_ACK);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_WAITSYSUI);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_rsp
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_rsp(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_RSP);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    log_debug("RSP [%03d] %ld -> %p  BUTTON(%d)\n", i, (long)vec[i], eve,
           eve->response);

    server_event_do_response_actions(eve);

    if( queue_get_event_state(eve) != ALARM_STATE_SNOOZED )
    {
      queue_set_event_state(eve, ALARM_STATE_SERVED);
    }
  }
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_snoozed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_snoozed(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;
  time_t    now = server_rethink_time;

  vec = queue_query_by_state(&cnt, ALARM_STATE_SNOOZED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    time_t snooze = server_event_get_snooze(eve);
    time_t prev   = eve->trigger;
    time_t curr   = ticker_bump_time(prev, snooze, now);

#if SNOOZE_HIJACK_FIX
    if( eve->snooze_total == 0 )
    {
      eve->snooze_total = eve->trigger;
    }
#else
    time_t add    = curr - prev;
    eve->snooze_total += add;
    log_debug("SNOOZE: %+d -> %+d\n", add, eve->snooze_total);
#endif

    queue_set_event_trigger(eve, curr);
    queue_set_event_state(eve, ALARM_STATE_NEW);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_served
 * ------------------------------------------------------------------------- */

static
void
server_rethink_served(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SERVED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    log_debug("%s: RECURRING = %s\n", __FUNCTION__,
              alarm_event_is_recurring(eve) ? "YES": "NO");

    if( alarm_event_is_recurring(eve) )
    {
      queue_set_event_state(eve, ALARM_STATE_RECURRING);
      continue;
    }
    queue_set_event_state(eve, ALARM_STATE_DELETED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_recurring
 * ------------------------------------------------------------------------- */

static
void
server_rethink_recurring(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_RECURRING);
  time_t    now = server_rethink_time;

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    const char *tz = server_event_get_tz(eve);

// QUARANTINE     log_debug("%s: RECURRING = %s\n", __FUNCTION__,
// QUARANTINE               alarm_event_is_recurring(eve) ? "YES": "NO");

    time_t prev = eve->trigger;
    time_t curr = INT_MAX;

    if( eve->recur_count > 0 )
    {
      eve->recur_count -= 1;
    }
#if SNOOZE_HIJACK_FIX
    prev = eve->snooze_total ?: eve->trigger;
    eve->snooze_total = 0;
#else
    prev -= eve->snooze_total;
    eve->snooze_total = 0;
#endif

// QUARANTINE     log_debug("RECURRING COUNT: %d\n", eve->recur_count);

    if( eve->recur_count != 0 )
    {
      if( eve->recur_secs > 0 )
      {
        curr = ticker_bump_time(prev, eve->recur_secs, now);
      }
      else
      {
        struct tm tm_now;

        ticker_break_tm(now, &tm_now, tz);
        //ticker_get_remote(server_rethink_time, server_tz_curr, &tm_now);

        for( size_t i = 0; i < eve->recurrence_cnt; ++i )
        {
          struct tm tm_tmp = tm_now;
          alarm_recur_t *rec = &eve->recurrence_tab[i];
          // TODO: think this over one more time
          time_t t = alarm_recur_next(rec, &tm_tmp, tz);
          //time_t t = alarm_recur_align(rec, &tm_tmp, tz);
          //log_debug_L("t=%d\n", t);
          if( t > 0 && t < curr ) curr = t;
        }
        //log_debug_L("t1=%d\n", curr);
      }
    }

    if( 0 < curr && curr < INT_MAX )
    {
      queue_set_event_trigger(eve, curr);
      queue_set_event_state(eve, ALARM_STATE_NEW);
    }
    else
    {
      queue_set_event_state(eve, ALARM_STATE_DELETED);
    }
  }
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_deleted
 * ------------------------------------------------------------------------- */

static
void
server_rethink_deleted(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_DELETED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    log_debug("DEL [%03d] %ld -> %p\n", i, (long)vec[i], eve);
    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DELETED);
  }

  if( cnt != 0 )
  {
    systemui_cancel_dialog(server_system_bus, vec, cnt);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_timezone
 * ------------------------------------------------------------------------- */

static
void
server_rethink_timezone(void)
{
  if( server_state_get() & SF_TZ_CHANGED )
  {
    server_state_clr(SF_TZ_CHANGED);
    log_debug("TIMEZONE: '%s' -> '%s'\n", server_tz_prev, server_tz_curr);
    server_timestate_sync();

    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);

      /* - - - - - - - - - - - - - - - - - - - *
       * do not re-schedule alarms that have
       * been snoozed
       * - - - - - - - - - - - - - - - - - - - */

      if( server_event_is_snoozed(eve) )
      {
        continue;
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * timezone change affects alarms that:
       * 1) use broken down alarm time
       * 2) use empty alarm timezone
       * - - - - - - - - - - - - - - - - - - - */

      if( (eve->alarm_time <= 0) && xisempty(eve->alarm_tz) )
      {
        time_t now = server_rethink_time;
        time_t old = eve->trigger;
        time_t use = server_event_get_next_trigger(now, -1, eve);

        if( use != old )
        {
          const char *tz = server_event_get_tz(eve);
          struct tm tm;
          char trg[128];

          ticker_break_tm(old, &tm, tz);
          tm_repr(&tm, tz, trg, sizeof trg);
          log_debug("OLD: %s, at %+d\n", trg, (int)(old - now));

          ticker_break_tm(use, &tm, tz);
          tm_repr(&tm, tz, trg, sizeof trg);
          log_debug("USE: %s, at %+d\n", trg, (int)(use - now));

          queue_set_event_trigger(eve, use);
          queue_set_event_state(eve, ALARM_STATE_NEW);
        }
      }
    }

    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_back_in_time
 * ------------------------------------------------------------------------- */

static
void
server_rethink_back_in_time(void)
{
  if( server_state_get() & SF_CLK_MV_BACK )
  {
    log_debug("HANDLE CLOCK -> BACKWARDS\n");
    server_state_clr(SF_CLK_MV_BACK);

    /* Re-evaluate trigger time for recurring alarms
     * with ALARM_EVENT_BACK_RESCHEDULE flag set */

// QUARANTINE     time_t    now = server_rethink_time;
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);
#if SNOOZE_ADJUST_FIX
    time_t    add = server_clock_back_delta();
#endif

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);

      /* - - - - - - - - - - - - - - - - - - - *
       * do not re-schedule alarms that have
       * been snoozed
       * - - - - - - - - - - - - - - - - - - - */

      if( server_event_is_snoozed(eve) )
      {
#if SNOOZE_ADJUST_FIX
        queue_set_event_trigger(eve, eve->trigger + add);
        queue_set_event_state(eve, ALARM_STATE_NEW);
#endif
        continue;
      }

      if( eve->flags & ALARM_EVENT_BACK_RESCHEDULE )
      {
        if( alarm_event_is_recurring(eve) )
        {
          // make moving recurrence time due to clock
          // change recur_count -neutral
          if( eve->recur_count > 0 )
          {
            eve->recur_count += 1;
          }
          queue_set_event_state(eve, ALARM_STATE_RECURRING);
        }
        else if( eve->alarm_time <= 0 )
        {
          // re-evalueate the trigger time if
          // the alarm uses broken down time
          server_event_set_trigger(eve);
          queue_set_event_state(eve, ALARM_STATE_NEW);
        }
      }
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_forw_in_time
 * ------------------------------------------------------------------------- */

static
void
server_rethink_forw_in_time(void)
{
  if( server_state_get() & SF_CLK_MV_FORW )
  {
    log_debug("HANDLE CLOCK -> FORWARDS\n");
    server_state_clr(SF_CLK_MV_FORW);

    /* Re-evaluate trigger time for recurring alarms
     * that were missed due to clock change */

    time_t    now = server_rethink_time;
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);
#if SNOOZE_ADJUST_FIX
    time_t    add = server_clock_forw_delta();
#endif

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);

      /* - - - - - - - - - - - - - - - - - - - *
       * do not re-schedule alarms that have
       * been snoozed
       * - - - - - - - - - - - - - - - - - - - */

      if( server_event_is_snoozed(eve) )
      {
#if SNOOZE_ADJUST_FIX
        queue_set_event_trigger(eve, eve->trigger + add);
        queue_set_event_state(eve, ALARM_STATE_NEW);
#endif
        continue;
      }

// QUARANTINE       if( eve->flags & ALARM_EVENT_FORW_RESCHEDULE )
// QUARANTINE       {
        if( alarm_event_is_recurring(eve) )
        {
          if( eve->trigger <= now )
          {
            // make moving recurrence time due to clock
            // change recur_count -neutral
            if( eve->recur_count > 0 )
            {
              eve->recur_count += 1;
            }
            queue_set_event_state(eve, ALARM_STATE_RECURRING);
          }
        }
// QUARANTINE       }
    }

    free(vec);
  }
}

static void server_broadcast_timechange_handled(void)
{
  if( server_state_get() & SF_CLK_BCAST)
  {
    server_state_clr(SF_CLK_BCAST);

    dbusif_signal_send(server_session_bus,
                       ALARMD_PATH,
                       ALARMD_INTERFACE,
                       ALARMD_TIME_CHANGE_IND,
                       DBUS_TYPE_INVALID);

    dbusif_signal_send(server_system_bus,
                       ALARMD_PATH,
                       ALARMD_INTERFACE,
                       ALARMD_TIME_CHANGE_IND,
                       DBUS_TYPE_INVALID);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_start_cb
 * ------------------------------------------------------------------------- */

static
gboolean
server_rethink_start_cb(gpointer data)
{
  log_debug("----------------------------------------------------------------\n");

  if( !server_clock_source_is_stable() )
  {
    server_state_set(0, SF_CLK_BCAST);
    server_cancel_wakeup();

    // broadcast queue status after time is stable
    server_queuestate_invalidate();

    server_rethink_id = g_timeout_add(1*1000, server_rethink_start_cb, 0);
    return FALSE;
  }

  server_rethink_id   = 0;
  server_rethink_time = ticker_get_time();

  server_rethink_back_in_time();
  server_rethink_forw_in_time();
  server_rethink_timezone();

  server_queue_cancel_save();

  log_debug("\n");
  log_debug("@ %s\n", __FUNCTION__);
  do
  {
    queue_clr_dirty();

    server_queuestate_reset();

    log_debug("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    server_rethink_new();
    server_rethink_waitconn();
    server_rethink_queued();
    server_rethink_missed();
    server_rethink_postponed();

    server_rethink_limbo();
    server_rethink_triggered();
    server_rethink_waitsysui();
    server_rethink_sysui_req();
    server_rethink_sysui_ack();
    server_rethink_sysui_rsp();

    server_rethink_snoozed();
    server_rethink_served();
    server_rethink_recurring();
    server_rethink_deleted();

    queue_cleanup_deleted();

    log_debug(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

  } while( queue_is_dirty() );
  //log_debug("\n");

// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_LIMBO);
// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_TRIGGERED);
  server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_WAITSYSUI);
  server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_SYSUI_REQ);
  server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_SYSUI_ACK);
// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_SYSUI_RSP);

  server_state_sync();

  if( server_state_get() & SF_SEND_POWERUP )
  {
    if( server_state_get() & SF_DSME_UP )
    {
      log_info("sending power up request to dsme\n");
      server_state_clr(SF_SEND_POWERUP);
      server_queue_forced_save();
      dsme_req_powerup(server_system_bus);
    }
    else
    {
      log_warning("can't send powerup request - dsme is not up\n");
    }
  }

  server_queuestate_sync();
  server_broadcast_timechange_handled();
  server_queue_request_save();

  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_rethink_request
 * ------------------------------------------------------------------------- */

static
void
server_rethink_request(void)
{
  if( server_rethink_id == 0 )
  {
    server_rethink_req = 0;
    server_rethink_id = g_idle_add(server_rethink_start_cb, 0);
  }
  else
  {
    server_rethink_req = 1;
  }
}

/* ========================================================================= *
 * DBUS MESSAGE HANDLING
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_handle_set_debug  --  set debug state
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_set_debug(DBusMessage *msg)
{
  uint32_t ms = 0;
  uint32_t mc = 0;
  uint32_t fs = 0;
  uint32_t fc = 0;

  DBusMessage  *rsp = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_UINT32, &ms,
                                       DBUS_TYPE_UINT32, &mc,
                                       DBUS_TYPE_UINT32, &fs,
                                       DBUS_TYPE_UINT32, &fc,
                                       DBUS_TYPE_INVALID)) )
  {
    server_state_mask |=  ms;
    server_state_mask &= ~mc;
    server_state_fake |=  fs;
    server_state_fake &= ~fc;

    ms = server_state_get();

    rsp = dbusif_reply_create(msg, DBUS_TYPE_UINT32, &ms, DBUS_TYPE_INVALID);

    server_state_set(0,0);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_CUD  --  "clear user data" handler
 * ------------------------------------------------------------------------- */

#if ALARMD_CUD_ENABLE
static
DBusMessage *
server_handle_CUD(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_int32_t  nak = 0;

  int       cnt = 0;
  cookie_t *vec = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * reset default snooze value
   * - - - - - - - - - - - - - - - - - - - */

  queue_set_snooze(0);

  /* - - - - - - - - - - - - - - - - - - - *
   * set all events deleted
   * - - - - - - - - - - - - - - - - - - - */

  if( (vec = queue_query_events(&cnt, 0,0, 0,0, 0)) )
  {
    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_set_event_state(eve, ALARM_STATE_DELETED);
    }
    free(vec);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * remove deleted events - no state
   * transition messages are generated
   * - - - - - - - - - - - - - - - - - - - */

  queue_cleanup_deleted();

  /* - - - - - - - - - - - - - - - - - - - *
   * save twice so that the backup gets
   * cleared too
   * - - - - - - - - - - - - - - - - - - - */

  server_queue_forced_save();
  server_queue_forced_save();

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &nak, DBUS_TYPE_INVALID);
  return rsp;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_handle_RFS  --  "restore factory settings" handler
 * ------------------------------------------------------------------------- */

#if ALARMD_RFS_ENABLE
static
DBusMessage *
server_handle_RFS(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_int32_t  nak = 0;

  queue_set_snooze(0);
  server_queue_forced_save();

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &nak, DBUS_TYPE_INVALID);
  return rsp;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_handle_snooze_get  --  handle ALARMD_SNOOZE_GET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_snooze_get(DBusMessage *msg)
{
  dbus_uint32_t val = queue_get_snooze();
  return dbusif_reply_create(msg, DBUS_TYPE_UINT32, &val, DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * server_handle_snooze_set  --  handle ALARMD_SNOOZE_SET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_snooze_set(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_uint32_t val = 0;
  dbus_bool_t   res = 1;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_UINT32, &val,
                                       DBUS_TYPE_INVALID)) )
  {
    queue_set_snooze(val);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);
  }

  server_queue_request_save();
  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_add  --  handle ALARMD_EVENT_ADD method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_add(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  cookie_t       cookie = 0;
  alarm_event_t *event  = 0;

  if( (event = dbusif_decode_event(msg)) != 0 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * client -> server: reset fields that
     * are manged by the server
     * - - - - - - - - - - - - - - - - - - - */

    event->cookie   = 0;
    event->trigger  = 0;
    event->response = -1;

    queue_set_event_state(event, ALARM_STATE_NEW);

    event->snooze_total = 0;

    if( server_event_set_trigger(event) == 0 )
    {
      if( (cookie = queue_add_event(event)) != 0 )
      {
        event = 0;
      }
    }
  }

  alarm_event_delete(event);

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &cookie, DBUS_TYPE_INVALID);

  server_rethink_request();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_update  --  handle ALARMD_EVENT_UPDATE method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_update(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  cookie_t       cookie = 0;
  alarm_event_t *event  = 0;

  if( (event = dbusif_decode_event(msg)) != 0 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * remove if exists
     * - - - - - - - - - - - - - - - - - - - */

    queue_del_event(event->cookie);

    /* - - - - - - - - - - - - - - - - - - - *
     * continue as with add
     * - - - - - - - - - - - - - - - - - - - */

    event->cookie   = 0;
    event->trigger  = 0;
    event->response = -1;

    queue_set_event_state(event, ALARM_STATE_NEW);

    event->snooze_total = 0;

    if( server_event_set_trigger(event) == 0 )
    {
      if( (cookie = queue_add_event(event)) != 0 )
      {
        event = 0;
      }
    }
  }

  alarm_event_delete(event);

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &cookie, DBUS_TYPE_INVALID);

  server_rethink_request();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_del  -- handle ALARMD_EVENT_DEL method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_del(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  dbus_bool_t    res    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &cookie,
                                       DBUS_TYPE_INVALID)) )
  {
    res = queue_del_event(cookie);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);
    server_rethink_request();
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_query  --  handle ALARMD_EVENT_QUERY method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_query(DBusMessage *msg)
{
  DBusMessage   *rsp  = 0;
  dbus_int32_t   lo   = 0;
  dbus_int32_t   hi   = 0;
  dbus_int32_t   mask = 0;
  dbus_int32_t   flag = 0;
  cookie_t      *vec  = 0;
  int            cnt  = 0;
  char          *app  = 0;

  assert( sizeof(dbus_int32_t) == sizeof(cookie_t) );

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &lo,
                                       DBUS_TYPE_INT32,  &hi,
                                       DBUS_TYPE_INT32,  &mask,
                                       DBUS_TYPE_INT32,  &flag,
                                       DBUS_TYPE_STRING, &app,
                                       DBUS_TYPE_INVALID)) )
  {
    if( (vec = queue_query_events(&cnt, lo, hi, mask, flag, app)) )
    {
      rsp = dbusif_reply_create(msg,
                                DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &vec, cnt,
                                DBUS_TYPE_INVALID);
    }
  }

  free(vec);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_get  --  handle ALARMD_EVENT_GET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_get(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  alarm_event_t *event  = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &cookie,
                                       DBUS_TYPE_INVALID)) )
  {
    if( (event = queue_get_event(cookie)) != 0 )
    {
      rsp = dbusif_reply_create(msg, DBUS_TYPE_INVALID);
      dbusif_encode_event(rsp, event, 0);
    }
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_ack  -- handle ALARMD_DIALOG_RSP method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_ack(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  dbus_int32_t   button = 0;
  dbus_bool_t    res    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &cookie,
                                       DBUS_TYPE_INT32,  &button,
                                       DBUS_TYPE_INVALID)) )
  {
    if( button != -1 && (button & SERVER_POWERUP_BIT) )
    {
      button &= ~SERVER_POWERUP_BIT;
      server_state_set(0, SF_SEND_POWERUP);
    }
    res = (server_handle_systemui_rsp(cookie, button) != -1);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);

    server_rethink_request();
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_queue_ack  -- handle ALARMD_DIALOG_ACK method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_queue_ack(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_bool_t    res    = 0;
  dbus_int32_t  *vec    = 0;
  int            cnt    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {
    res = (server_handle_systemui_ack((cookie_t *)vec, cnt) != -1);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);

  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_name_acquired
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_name_acquired(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;
  char          *service   = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_STRING, &service,
                                       DBUS_TYPE_INVALID)) )
  {
    log_debug("NAME ACQUIRED: '%s'\n", service);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_save_data
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_save_data(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_debug("================ SAVE DATA SIGNAL ================\n");

  server_queue_forced_save();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_shutdown
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_shutdown(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_debug("================ SHUT DOWN SIGNAL ================\n");

  // FIXME: what are we supposed to do when dsme sends us the shutdown signal?

// QUARANTINE   server_queue_forced_save();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_name_owner_chaned  --  handle dbus name owner changes
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_name_owner_chaned(DBusMessage *msg)
{

  DBusMessage   *rsp       = 0;
  char          *service   = 0;
  char          *old_owner = 0;
  char          *new_owner = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_STRING, &service,
                                       DBUS_TYPE_STRING, &old_owner,
                                       DBUS_TYPE_STRING, &new_owner,
                                       DBUS_TYPE_INVALID)) )
  {
    log_debug("NAME OWNER CHANGED: '%s': '%s' -> '%s'\n",
              service, old_owner, new_owner);

    unsigned clr = 0, set = 0;

    if( !strcmp(service, SYSTEMUI_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_SYSTEMUI_UP;
        set |= SF_SYSTEMUI_DN;
      }
      else
      {
        set |= SF_SYSTEMUI_UP;
      }
    }

    if( !strcmp(service, FAKESYSTEMUI_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_FAKEUI_UP;
        set |= SF_FAKEUI_DN;
      }
      else
      {
        set |= SF_FAKEUI_UP;
      }
    }

    if( !strcmp(service, CLOCKD_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_CLOCKD_UP;
        set |= SF_CLOCKD_DN;

        ticker_use_libtime(FALSE);
      }
      else
      {
        set |= SF_CLOCKD_UP;

        ticker_use_libtime(TRUE);
      }
    }

    if( !strcmp(service, DSME_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_DSME_UP;
        set |= SF_DSME_DN;
      }
      else
      {
        set |= SF_DSME_UP;
      }
    }

    if( !strcmp(service, MCE_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_MCE_UP;
        set |= SF_MCE_DN;
      }
      else
      {
        set |= SF_MCE_UP;
      }
    }

    if( !strcmp(service, STATUSAREA_CLOCK_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_STATUSBAR_UP;
        set |= SF_STATUSBAR_DN;
      }
      else
      {
        set |= SF_STATUSBAR_UP;

// QUARANTINE         log_debug("statusarea clock plugin detected -> LIMBO disabled\n");
// QUARANTINE         set |= SF_DESKTOP_UP;
      }
    }

    if( !strcmp(service, DESKTOP_SERVICE) )
    {
      log_debug("desktop ready detected -> LIMBO disabled\n");
      set |= SF_DESKTOP_UP;
    }

    server_state_set(clr, set);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_time_change
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_time_change(DBusMessage *msg)
{
  DBusMessage *rsp = 0;
  dbus_int32_t clk = 0;
  DBusError    err = DBUS_ERROR_INIT;

  log_debug("################ CLOCKD NOTIFICATION ################\n");

  server_state_set(0, SF_CLK_BCAST);

  if( !dbus_message_get_args(msg, &err,
                             DBUS_TYPE_INT32, &clk,
                             DBUS_TYPE_INVALID) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  ticker_get_synced();

  server_timestate_read();

  if( strcmp(server_tz_curr, server_tz_prev) )
  {
    log_debug("################ TIMEZONE CHANGED ################\n");
    server_state_set(0, SF_TZ_CHANGED);
  }

  if( server_dst_curr != server_dst_prev )
  {
    log_debug("################ ISDST CHANGED ################\n");
    server_state_set(0, SF_TZ_CHANGED);
  }

  if( clk != 0 )
  {
    log_debug("################ CLOCK CHANGED ################\n");
    server_state_set(0, SF_CLK_CHANGED);
  }

  dbus_error_free(&err);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_session_bus_cb  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
server_session_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
#if ALARMD_ON_SESSION_BUS
  static const dbusif_method_lut alarm_methods[] =
  {
    {ALARMD_EVENT_ADD,   server_handle_event_add},
    {ALARMD_EVENT_DEL,   server_handle_event_del},
    {ALARMD_EVENT_GET,   server_handle_event_get},
    {ALARMD_EVENT_QUERY, server_handle_event_query},
    {ALARMD_EVENT_UPDATE,server_handle_event_update},

    {ALARMD_SNOOZE_SET,  server_handle_snooze_set},
    {ALARMD_SNOOZE_GET,  server_handle_snooze_get},

    {ALARMD_DIALOG_RSP,  server_handle_event_ack},
    {ALARMD_DIALOG_ACK,  server_handle_queue_ack},

    {"alarmd_set_debug", server_handle_set_debug},

#if ALARMD_CUD_ENABLE
    {"clear_user_data",          server_handle_CUD},
#endif
#if ALARMD_RFS_ENABLE
    {"restore_factory_settings", server_handle_RFS},
#endif

    {0,}
  };
#endif

  static const dbusif_method_lut dbus_signals[] =
  {
    {DBUS_NAME_OWNER_CHANGED, server_handle_name_owner_chaned},
    {DBUS_NAME_ACQUIRED,      server_handle_name_acquired},
    {0,}
  };

  static const dbusif_interface_lut filter[] =
  {
#if ALARMD_ON_SESSION_BUS
    {
      ALARMD_INTERFACE,
      ALARMD_PATH,
      DBUS_MESSAGE_TYPE_METHOD_CALL,
      alarm_methods,
      DBUS_HANDLER_RESULT_HANDLED
    },
#endif

    {
      DBUS_INTERFACE_DBUS,
      DBUS_PATH_DBUS,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dbus_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    { 0, }
  };

  return dbusif_handle_message_by_interface(filter, conn, msg);
}

/* ------------------------------------------------------------------------- *
 * server_system_bus_cb
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
server_system_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
#if ALARMD_ON_SYSTEM_BUS
  static const dbusif_method_lut alarm_methods[] =
  {
    {ALARMD_EVENT_ADD,   server_handle_event_add},
    {ALARMD_EVENT_DEL,   server_handle_event_del},
    {ALARMD_EVENT_GET,   server_handle_event_get},
    {ALARMD_EVENT_QUERY, server_handle_event_query},
    {ALARMD_EVENT_UPDATE,server_handle_event_update},

    {ALARMD_SNOOZE_SET,  server_handle_snooze_set},
    {ALARMD_SNOOZE_GET,  server_handle_snooze_get},

    {ALARMD_DIALOG_RSP,  server_handle_event_ack},
    {ALARMD_DIALOG_ACK,  server_handle_queue_ack},

    {"alarmd_set_debug", server_handle_set_debug},

#if ALARMD_CUD_ENABLE
    {"clear_user_data",          server_handle_CUD},
#endif
#if ALARMD_RFS_ENABLE
    {"restore_factory_settings", server_handle_RFS},
#endif

    {0,}
  };
#endif

  static const dbusif_method_lut clockd_signals[] =
  {
    {CLOCKD_TIME_CHANGED,   server_handle_time_change},
    {0,}
  };

  static const dbusif_method_lut dbus_signals[] =
  {
    {DBUS_NAME_OWNER_CHANGED, server_handle_name_owner_chaned},
    {DBUS_NAME_ACQUIRED,      server_handle_name_acquired},
    {0,}
  };

  static const dbusif_method_lut dsme_signals[] =
  {
    {DSME_DATA_SAVE_SIG,   server_handle_save_data},
    {DSME_SHUTDOWN_SIG,    server_handle_shutdown},
    {0,}
  };

  static const dbusif_interface_lut filter[] =
  {
#if ALARMD_ON_SYSTEM_BUS
    {
      ALARMD_INTERFACE,
      ALARMD_PATH,
      DBUS_MESSAGE_TYPE_METHOD_CALL,
      alarm_methods,
      DBUS_HANDLER_RESULT_HANDLED
    },
#endif
    {
      CLOCKD_INTERFACE,
      CLOCKD_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      clockd_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    {
      DBUS_INTERFACE_DBUS,
      DBUS_PATH_DBUS,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dbus_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    {
      DSME_SIGNAL_IF,
      DSME_SIGNAL_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dsme_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    { 0, }
  };

  return dbusif_handle_message_by_interface(filter, conn, msg);
}

/* ------------------------------------------------------------------------- *
 * server_icd_status_cb
 * ------------------------------------------------------------------------- */

static
void
server_icd_status_cb(int connected)
{
  if( connected )
  {
    log_debug("CON_IC_STATUS == CONNECTED\n");
    server_state_set(0, SF_CONNECTED);
  }
  else
  {
    log_debug("CON_IC_STATUS != CONNECTED\n");
    server_state_set(SF_CONNECTED, 0);
  }
}

/* ========================================================================= *
 * SERVER INIT/QUIT
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_init_session_bus
 * ------------------------------------------------------------------------- */

static
int
server_init_session_bus(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  // connect
  if( (server_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    goto cleanup;
  }

  // register filter callback
  if( !dbus_connection_add_filter(server_session_bus, server_session_bus_cb, 0, 0) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, "add filter", "FAILED");
    goto cleanup;
  }

  // listen to signals
  if( dbusif_add_matches(server_session_bus, sessionbus_signals) == -1 )
  {
    goto cleanup;
  }

  // bind to gmainloop
  dbus_connection_setup_with_g_main(server_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(server_session_bus, 0);

  // claim service name
  int ret = dbus_bus_request_name(server_session_bus, ALARMD_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    }
    else
    {
      log_error("%s: %s: %s\n", __FUNCTION__, "request name",
                "not primary owner of connection\n");
    }
    goto cleanup;
  }

  // success

  res = 0;

  // cleanup & return

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * server_init_system_bus
 * ------------------------------------------------------------------------- */

static
int
server_init_system_bus(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  // connect
  if( (server_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    if( running_in_scratchbox() )
    {
      fprintf(stderr, "scratchbox detected: system bus actions not available\n");
      res = 0;
    }
    goto cleanup;
  }

  // register filter callback
  if( !dbus_connection_add_filter(server_system_bus, server_system_bus_cb, 0, 0) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, "add filter", "FAILED");
    goto cleanup;
  }

  // listen to signals
  if( dbusif_add_matches(server_system_bus, systembus_signals) == -1 )
  {
    goto cleanup;
  }

  // bind to gmainloop
  dbus_connection_setup_with_g_main(server_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(server_system_bus, 0);

  // claim service name
  int ret = dbus_bus_request_name(server_system_bus, ALARMD_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    }
    else
    {
      log_error("%s: %s: %s\n", __FUNCTION__, "request name",
                "not primary owner of connection\n");
    }
    goto cleanup;
  }

  // success

  res = 0;

  // cleanup & return

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * server_quit_session_bus
 * ------------------------------------------------------------------------- */

static
void
server_quit_session_bus(void)
{
  if( server_session_bus != 0 )
  {
    dbusif_remove_matches(server_session_bus, sessionbus_signals);
    dbus_connection_remove_filter(server_session_bus, server_session_bus_cb, 0);
    dbus_connection_unref(server_session_bus);
    server_session_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_quit_system_bus
 * ------------------------------------------------------------------------- */

static
void
server_quit_system_bus(void)
{
  if( server_system_bus != 0 )
  {
    dbusif_remove_matches(server_system_bus, systembus_signals);
    dbus_connection_remove_filter(server_system_bus, server_system_bus_cb, 0);
    dbus_connection_unref(server_system_bus);
    server_system_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_init
 * ------------------------------------------------------------------------- */

int
server_init(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * even if alarmd is run as root, the
   * execute actions are performed as
   * 'user' or 'nobody'
   * - - - - - - - - - - - - - - - - - - - */

  ipc_exec_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * setup system ui response callback
   * - - - - - - - - - - - - - - - - - - - */

  systemui_set_ack_callback(server_systemui_ack_cb);
  systemui_set_service_callback(server_state_get_systemui_service);

  /* - - - - - - - - - - - - - - - - - - - *
   * session & system bus connections
   * - - - - - - - - - - - - - - - - - - - */

  if( server_init_system_bus() == -1 )
  {
    goto cleanup;
  }

  if( server_init_session_bus() == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * start icd state tracking
   * - - - - - - - - - - - - - - - - - - - */

  if( server_system_bus != 0 )
  {
    // conic signalling uses system bus
    ipc_icd_init(server_icd_status_cb);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * set the ball rolling
   * - - - - - - - - - - - - - - - - - - - */

  server_state_init();
  server_queuestate_init();

  server_rethink_request();

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  res = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * server_quit
 * ------------------------------------------------------------------------- */

void
server_quit(void)
{
  server_queue_cancel_save();

  ipc_icd_quit();

  server_quit_session_bus();
  server_quit_system_bus();

  ipc_exec_quit();
}