#ifndef __agh_ubus_h__
#define __agh_ubus_h__

#include <libubus.h>
#include <glib.h>

#define AGH_UBUS_UNIX_SOCKET "/var/run/ubus.sock"
#define AGH_UBUS_POLL_INTERVAL 800

/* ubus calls failure reasons */
#define AGH_UBUS_CALL_MISSING_PATH 1
#define AGH_UBUS_CALL_MISSING_METHOD 2
#define AGH_UBUS_CALL_BLOB_BUF_INIT_FAILURE 3
#define AGH_UBUS_CALL_INVALID_JSON_MESSAGE 4
#define AGH_UBUS_CALL_METHOD_NOT_FOUND 5
/* end of ubus calls failure reasons */

/* agh_ubus_handle_events states */
#define AGH_UBUS_STATE_INIT 0
#define AGH_UBUS_STATE_CONNECTED 1
#define AGH_UBUS_STATE_RECONNECTING 2
#define AGH_UBUS_STATE_STOP 3
/* end of agh_ubus_handle_events states */

struct agh_ubus_ctx {
	GMainContext *gmctx;
	GSource *agh_ubus_timeoutsrc;
	guint agh_ubus_timeoutsrc_tag;
	struct ubus_context *ctx;
	struct ubus_event_handler *event_handler;
	GQueue *event_masks;
	struct agh_ubus_logstream_ctx *logstream_ctx;
};

extern gchar *agh_ubus_call_data_str;
extern gint agh_ubus_connection_state;
extern struct agh_comm *agh_ubus_aghcomm;

struct agh_ubus_ctx *agh_ubus_setup(struct agh_comm *comm, gint *retvptr);
void agh_ubus_teardown(struct agh_ubus_ctx *uctx);
gint agh_ubus_call(struct agh_ubus_ctx *uctx, const gchar *path, const gchar *method, const gchar *message);
gchar *agh_ubus_get_call_result(gboolean dup);

/* ubus events */
gint agh_ubus_event_add(struct agh_ubus_ctx *uctx, void (*cb)(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg), const gchar *mask);
gint agh_ubus_event_disable(struct agh_ubus_ctx *uctx);

#endif
