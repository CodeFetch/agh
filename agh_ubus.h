#ifndef __agh_ubus_h__
#define __agh_ubus_h__

#include <libubus.h>
#include <glib.h>

#define AGH_UBUS_UNIX_SOCKET "/var/run/ubus.sock"
#define AGH_UBUS_POLL_INTERVAL 4*G_USEC_PER_SEC

struct agh_ubus_ctx {
	GMainContext *gmctx;
	GSource *agh_ubus_timeoutsrc;
	guint agh_ubus_timeoutsrc_tag;
	struct ubus_context *ctx;
};

extern gchar *agh_ubus_call_data_str;

struct agh_ubus_ctx *agh_ubus_setup(GMainContext *gmctx);
void agh_ubus_teardown(struct agh_ubus_ctx *uctx);
gboolean agh_ubus_handle_events(gpointer data);
void agh_receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg);

#endif
