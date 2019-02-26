#include <glib-unix.h>
#include <libubox/blobmsg_json.h>
#include "agh_ubus.h"
#include "agh_messages.h"
#include "agh_ubus_logstream.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_UBUS domain. */
#define AGH_LOG_DOMAIN_UBUS	"UBUS"

/* Logging macros. */
#define agh_log_ubus_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_UBUS, message, ##__VA_ARGS__)
#define agh_log_ubus_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_UBUS, message, ##__VA_ARGS__)

gchar *agh_ubus_call_data_str;
gint agh_ubus_connection_state;
struct agh_comm *agh_ubus_aghcomm;

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

static void agh_ubus_disconnect_cb(struct ubus_context *ctx) {
	agh_ubus_connection_state++;
	return;
}

static gboolean agh_ubus_handle_events(gpointer data) {
	struct agh_ubus_ctx *uctx = data;

	switch(agh_ubus_connection_state) {
		case AGH_UBUS_STATE_INIT:
			uctx->ctx = ubus_connect(AGH_UBUS_UNIX_SOCKET);

			if (uctx->ctx) {
				agh_log_ubus_dbg("ubus connection established with local ID %08x",uctx->ctx->local_id);
				uctx->ctx->connection_lost = agh_ubus_disconnect_cb;
				agh_ubus_connection_state++;
			}

			break;
		case AGH_UBUS_STATE_CONNECTED:
			ubus_handle_event(uctx->ctx);
			break;
		case AGH_UBUS_STATE_RECONNECTING:
			if (uctx->logstream_ctx)
				if (uctx->logstream_ctx->logstream_state != 2)
					uctx->logstream_ctx->logstream_state = 3;

			if (!ubus_reconnect(uctx->ctx, AGH_UBUS_UNIX_SOCKET)) {
				agh_log_ubus_dbg("ubus connection re-established with local ID %08x",uctx->ctx->local_id);
				agh_ubus_connection_state--;
			}

			break;
		case AGH_UBUS_STATE_STOP:
			agh_log_ubus_dbg("AGH_UBUS_STATE_STOP, bye bye!");

			/* We can do this because we already called g_source_unref on this GSource. */
			uctx->agh_ubus_timeoutsrc = NULL;
			return FALSE;
		default:
			agh_log_ubus_crit("unknown state");
			agh_ubus_connection_state = AGH_UBUS_STATE_STOP;
	}

	return TRUE;
}

static void agh_receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg) {

	if (agh_ubus_call_data_str) {
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	if (!msg)
		return;

	agh_ubus_call_data_str = blobmsg_format_json_with_cb(msg, true, NULL, NULL, 0);
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

	ubus_invoke(uctx->ctx, id, method, bbuf->head, agh_receive_call_result_data, NULL, 80 * 1000);

	blob_buf_free(bbuf);
	g_free(bbuf);
	return 0;
}

/*
 * Returns a pointer to a string holding the data resulting from an ubus call.
 *
 * Optionally, this function will copy the text.
 *
 * Returns: a pointer pointing to the string (optionally the copied one) or NULL on failure.
 *
 * This function may terminate the program uncleanly.
*/
gchar *agh_ubus_get_call_result(gboolean dup) {
	gchar *res;

	res = NULL;

	if (agh_ubus_call_data_str) {

		if (dup) {
			res = g_strdup(agh_ubus_call_data_str);
			g_free(agh_ubus_call_data_str);
			agh_ubus_call_data_str = NULL;
		}
		else
			res = agh_ubus_call_data_str;

	}
	else
		agh_log_ubus_dbg("no data to return");

	return res;
}

/*
 * Initializes our connection to the ubus messaging bus.
 * Parameters: the AGH core's COMM, and a pointer to an integer value, where error values will be stored.
 * The pointed integer should be 0.
 *
 * Returns: a struct agh_ubus_ctx pointer is returned on success, NULL on failure.
 * NULL may be returned due to:
 *  - the specified COMM being NULL, or has a NULL GMainContext pointer
 *  - the retvptr pointer is NULL or points to a non-zero integer value
 * In those cases, *retvptr value is not set.
*
 * Failure values (*retvptr):
 *  - -1 = failure while allocating AGH ubus context
 *  - -2 = failure while allocating ubus event handler structure
 *  - -3 = GSource attach failure
 *
 * Note: this function may terminate the program uncleanly.
*/
struct agh_ubus_ctx *agh_ubus_setup(struct agh_comm *comm, gint *retvptr) {
	struct agh_ubus_ctx *uctx;
	struct ubus_event_handler *event_handler;

	uctx = NULL;

	if (!comm || !comm->ctx || !retvptr || *retvptr) {
		agh_log_ubus_dbg("specified COMM for the AGH core is NULL or has NULL GMainContext, retvptr is NULL or *retvptr is not 0");
		return uctx;
	}

	/* global vars */
	agh_ubus_connection_state = AGH_UBUS_STATE_INIT;
	agh_ubus_aghcomm = comm;
	agh_ubus_call_data_str = NULL;
	agh_ubus_aghcomm = NULL;

	uctx = g_try_malloc0(sizeof(*uctx));
	if (!uctx) {
		agh_log_ubus_crit("can not allocate AGH ubus context");
		*retvptr = -1;
		goto wayout;
	}

	event_handler = g_try_malloc0(sizeof(*event_handler));
	if (!event_handler) {
		agh_log_ubus_crit("can not allocate ubus event handler struct");
		*retvptr = -2;
		goto wayout;
	}

	uctx->gmctx = comm->ctx;
	uctx->event_handler = event_handler;

	uctx->agh_ubus_timeoutsrc = g_timeout_source_new(AGH_UBUS_POLL_INTERVAL);
	g_source_set_callback(uctx->agh_ubus_timeoutsrc, agh_ubus_handle_events, uctx, NULL);
	uctx->agh_ubus_timeoutsrc_tag = g_source_attach(uctx->agh_ubus_timeoutsrc, uctx->gmctx);
	if (!uctx->agh_ubus_timeoutsrc_tag) {
		agh_log_ubus_crit("error while attaching the ubus timeout source to GMainContext");
		g_source_destroy(uctx->agh_ubus_timeoutsrc);
		uctx->agh_ubus_timeoutsrc = NULL;
		*retvptr = -3;
		goto wayout;
	}

	g_source_unref(uctx->agh_ubus_timeoutsrc);

wayout:

	if (*retvptr) {

		if (uctx) {
			g_free(uctx->event_handler);
			g_free(uctx);
		}

	}

	return uctx;
}
