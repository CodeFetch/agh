#include <libubox/blobmsg_json.h>
#include "agh_ubus.h"

gchar *agh_ubus_call_data_str;

struct agh_ubus_ctx *agh_ubus_setup(GMainContext *gmctx) {
	struct agh_ubus_ctx *uctx;

	uctx = NULL;
	agh_ubus_call_data_str = NULL;

	uctx = g_malloc0(sizeof(struct agh_ubus_ctx));

	uctx->ctx = ubus_connect(AGH_UBUS_UNIX_SOCKET);

	if (!uctx->ctx) {
		g_free(uctx);
		uctx = NULL;
		return uctx;
	}

	uctx->gmctx = gmctx;

	uctx->agh_ubus_timeoutsrc = g_timeout_source_new(AGH_UBUS_POLL_INTERVAL);
	g_source_set_callback(uctx->agh_ubus_timeoutsrc, agh_ubus_handle_events, uctx, NULL);
	uctx->agh_ubus_timeoutsrc_tag = g_source_attach(uctx->agh_ubus_timeoutsrc, uctx->gmctx);
	g_source_unref(uctx->agh_ubus_timeoutsrc);

	return uctx;
}

void agh_ubus_teardown(struct agh_ubus_ctx *uctx) {

	if (!agh_ubus_call_data_str) {
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	if (!uctx)
		return;

	if (uctx->ctx) {
		ubus_free(uctx->ctx);
		uctx->ctx = NULL;
	}

	if (uctx->agh_ubus_timeoutsrc) {
		g_print("%s: source still present, please take a look\n",__FUNCTION__);
		g_source_destroy(uctx->agh_ubus_timeoutsrc);
		uctx->agh_ubus_timeoutsrc = NULL;
	}
	uctx->agh_ubus_timeoutsrc_tag = 0;

	g_free(uctx);

	return;
}

gboolean agh_ubus_handle_events(gpointer data) {
	struct agh_ubus_ctx *uctx = data;

	//ubus_handle_event(uctx->ctx);
	return TRUE;
}

void agh_receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg) {

	if (agh_ubus_call_data_str) {
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	if (!msg)
		return;

	agh_ubus_call_data_str = blobmsg_format_json_with_cb(msg, true, NULL, NULL, 0);
	return;
}
