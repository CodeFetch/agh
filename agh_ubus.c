#include <glib-unix.h>
#include <libubox/blobmsg_json.h>
#include "agh_ubus.h"
#include "agh_messages.h"
#include "agh_ubus_logstream.h"

gchar *agh_ubus_call_data_str;
gint agh_ubus_connection_state;
struct agh_comm *agh_ubus_aghcomm;

struct agh_ubus_ctx *agh_ubus_setup(struct agh_comm *comm) {
	struct agh_ubus_ctx *uctx;

	uctx = NULL;
	agh_ubus_call_data_str = NULL;
	agh_ubus_aghcomm = NULL;

	if (!comm) {
		g_print("%s: NULL comm\n",__FUNCTION__);
		return uctx;
	}

	if (!comm->ctx) {
		g_print("%s: NULL gmctx\n",__FUNCTION__);
		return uctx;
	}

	agh_ubus_connection_state = AGH_UBUS_STATE_INIT;

	uctx = g_malloc0(sizeof(struct agh_ubus_ctx));

	uctx->gmctx = comm->ctx;

	uctx->agh_ubus_timeoutsrc = g_timeout_source_new(AGH_UBUS_POLL_INTERVAL);
	g_source_set_callback(uctx->agh_ubus_timeoutsrc, agh_ubus_handle_events, uctx, NULL);
	uctx->agh_ubus_timeoutsrc_tag = g_source_attach(uctx->agh_ubus_timeoutsrc, uctx->gmctx);
	g_source_unref(uctx->agh_ubus_timeoutsrc);

	uctx->event_handler = g_malloc0(sizeof(struct ubus_event_handler));
	agh_ubus_aghcomm = comm;

	return uctx;
}

void agh_ubus_teardown(struct agh_ubus_ctx *uctx) {

	if (agh_ubus_call_data_str) {
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	agh_ubus_aghcomm = NULL;

	if (!uctx)
		return;

	if (uctx->ctx) {
		ubus_free(uctx->ctx);
		uctx->ctx = NULL;
	}

	if (uctx->agh_ubus_timeoutsrc) {
		g_source_destroy(uctx->agh_ubus_timeoutsrc);
		uctx->agh_ubus_timeoutsrc = NULL;
	}
	uctx->agh_ubus_timeoutsrc_tag = 0;

	if (uctx->event_masks) {
		g_queue_free_full(uctx->event_masks, g_free);
		uctx->event_masks = NULL;
	}

	if (uctx->event_handler) {
		g_free(uctx->event_handler);
		uctx->event_handler = NULL;
	}
	agh_ubus_connection_state = AGH_UBUS_STATE_INIT;

	if (uctx->logstream_ctx)
		agh_ubus_logstream_deinit(uctx);

	g_free(uctx);

	return;
}

gboolean agh_ubus_handle_events(gpointer data) {
	struct agh_ubus_ctx *uctx = data;

	switch(agh_ubus_connection_state) {
		case AGH_UBUS_STATE_INIT:
			uctx->ctx = ubus_connect(AGH_UBUS_UNIX_SOCKET);

			if (uctx->ctx) {
				g_print("%s: ubus connection established with local ID %08x\n",__FUNCTION__,uctx->ctx->local_id);
				uctx->ctx->connection_lost = agh_ubus_disconnect_cb;
				agh_ubus_connection_state++;
			}

			/* let's wait next iteration, evne if not necessary */
			break;
		case AGH_UBUS_STATE_CONNECTED:
			ubus_handle_event(uctx->ctx);
			break;
		case AGH_UBUS_STATE_RECONNECTING:
			if (uctx->logstream_ctx)
				if (uctx->logstream_ctx->logstream_state != 2)
					uctx->logstream_ctx->logstream_state = 3;

			if (!ubus_reconnect(uctx->ctx, AGH_UBUS_UNIX_SOCKET)) {
				g_print("%s: ubus connection re-established with local ID %08x\n",__FUNCTION__,uctx->ctx->local_id);
				agh_ubus_connection_state--;
			}

			break;
		case AGH_UBUS_STATE_STOP:
			g_print("%s: AGH_UBUS_STATE_STOP, bye bye!\n",__FUNCTION__);

			/* We can do this because we already called g_source_unref on this GSource. */
			uctx->agh_ubus_timeoutsrc = NULL;
			return FALSE;
		default:
			g_print("%s: unknown state\n",__FUNCTION__);
			agh_ubus_connection_state = AGH_UBUS_STATE_STOP;
	}

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

void agh_ubus_disconnect_cb(struct ubus_context *ctx) {
	agh_ubus_connection_state++;
	return;
}

gint agh_ubus_event_add(struct agh_ubus_ctx *uctx, void (*cb)(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg), const gchar *mask) {
	gint retval;
	gchar *mask_tmp;
	guint i;
	guint num_masks;

	retval = 0;
	mask_tmp = NULL;
	i = 0;
	num_masks = 0;

	if (!uctx || !mask)
		return retval;

	if (!cb)
		return retval;

	if (uctx->event_masks) {
		g_print("%s: searching for mask %s in event masks\n",__FUNCTION__,mask);
		num_masks = g_queue_get_length(uctx->event_masks);
		for (i=0;i<num_masks;i++) {
			mask_tmp = g_queue_peek_nth(uctx->event_masks, i);
			if (!g_strcmp0(mask_tmp, mask))
				return ++retval;
		}
	}

	uctx->event_handler->cb = cb;

	retval = ubus_register_event_handler(uctx->ctx, uctx->event_handler, mask);
	if (retval) {
		uctx->event_handler->cb = NULL;
		return retval;
	}

	if (!uctx->event_masks)
		uctx->event_masks = g_queue_new();

	g_queue_push_tail(uctx->event_masks, g_strdup(mask));

	return retval;
}

gint agh_ubus_event_disable(struct agh_ubus_ctx *uctx) {
	gint retval;

	retval = 10;

	if (!uctx)
		return retval;

	if (!uctx->event_masks)
		return retval;

	retval = ubus_unregister_event_handler(uctx->ctx, uctx->event_handler);
	if (retval)
		return retval;

	g_queue_free_full(uctx->event_masks, g_free);
	uctx->event_masks = NULL;
	uctx->event_handler->cb = NULL;

	return retval;
}

gint agh_ubus_call(struct agh_ubus_ctx *uctx, const gchar *path, const gchar *method, const gchar *message) {
	struct blob_buf *bbuf;
	guint32 id;

	bbuf = NULL;
	id = 0;

	if (agh_ubus_call_data_str) {
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	if (!path)
		return AGH_UBUS_CALL_MISSING_PATH;

	if (!method)
		return AGH_UBUS_CALL_MISSING_METHOD;

	bbuf = g_malloc0(sizeof(struct blob_buf));

	if (blob_buf_init(bbuf, 0)) {
		g_free(bbuf);
		return AGH_UBUS_CALL_BLOB_BUF_INIT_FAILURE;
	}

	if (message) {
		if (!blobmsg_add_json_from_string(bbuf, message)) {
			blob_buf_free(bbuf);
			g_free(bbuf);
			return AGH_UBUS_CALL_INVALID_JSON_MESSAGE;
		}
	}

	if (ubus_lookup_id(uctx->ctx, path, &id)) {
		blob_buf_free(bbuf);
		g_free(bbuf);
		return AGH_UBUS_CALL_METHOD_NOT_FOUND;
	}

	ubus_invoke(uctx->ctx, id, method, bbuf->head, agh_receive_call_result_data, NULL, 10 * 1000);

	blob_buf_free(bbuf);
	g_free(bbuf);
	return 0;
}

gchar *agh_ubus_get_call_result(void) {
	gchar *res;

	res = NULL;

	if (agh_ubus_call_data_str) {
		res = g_strdup(agh_ubus_call_data_str);
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	return res;
}
