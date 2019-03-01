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

/*
 * Disconnects from ubus and free resources. It also sets global pointers to NULL, and agh_ubus_connection_state to AGH_UBUS_STATE_INIT.
 *
 * Returns: an integer with value -1 when passed agh_ubus_ctx struct is NULL, 0 otherwise.
*/
gint agh_ubus_teardown(struct agh_ubus_ctx *uctx) {
	gint retval;

	retval = 0;

	g_free(agh_ubus_call_data_str);
	agh_ubus_call_data_str = NULL;
	agh_ubus_aghcomm = NULL;

	if (!uctx) {
		agh_log_ubus_dbg("NULL AGH ubus context");
		retval = -1;
		goto wayout;
	}

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

	if (uctx->logstream_ctx) {
		agh_ubus_logstream_deinit(uctx);
		uctx->logstream_ctx = NULL;
	}

	agh_ubus_connection_state = AGH_UBUS_STATE_INIT;

	g_free(uctx);

wayout:
	return retval;
}

/*
 * Invoked when ubus connection is lost.
 *
 * Returns: nothing.
*/
static void agh_ubus_disconnect_cb(struct ubus_context *ctx) {
	agh_ubus_connection_state++;
	agh_log_ubus_crit("we got disconnected!");
	return;
}

/*
 * This function is invoked by GLib, as a timeout GSource attached to a GMainContext.
 *
 * After a successful completion of the agh_ubus_setup function, we expect to be at state AGH_UBUS_STATE_INIT.
 * We'll execute the corresponding branch of the switch statemenet in the function, until a connection can be established.
 * Upon a successful connection, we install the agh_ubus_disconnect_cb handler as the "connection lost" one
 * (ctx->connection_lost) and enter the AGH_UBUS_STATE_CONNECTED switch branch at next invocation, where we handle events
 * invoking the related ubus function.
 * Should a disconnection occur, we expect the agh_ubus_disconnect_cb function to be invoked. (It will simply increment the agh_ubus_connection_state variable.)
 * At this point, we should be in the AGH_UBUS_STATE_RECONNECTING state, where we constantly try to reconnect to ubus. If logstreaming
 * is not connected, it's state is changed so it should deinit the log channel and start busy-waiting for us to (re-)establish
 * an ubus connection.
*/
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
			if (uctx->logstream_ctx && (uctx->logstream_ctx->logstream_state != 2))
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

/*
 * Invoked upon reception of data resulting from an ubus call.
 * Result is placed on the agh_ubus_call_data_str global pointer variable. If any content is stored there, it's freed.
 * After this function exits, agh_ubus_call_data_str may still be NULL.
 *
 * Returns: nothing.
*/
static void agh_receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg) {

	g_free(agh_ubus_call_data_str);
	agh_ubus_call_data_str = NULL;

	if (!msg)
		return;

	agh_ubus_call_data_str = blobmsg_format_json_with_cb(msg, true, NULL, NULL, 0);
	return;
}

/*
 * Registers an ubus event handler for a specified event mask, allocating a GQueue of masks strings if not already allocated.
 * New event masks strings are added to that GQueue if successfully added.
 * This function may terminate the program uncleanly.
 *
 * Returns: an integer with value 0 on success.
 *  - -1 = no AGH ubus context, NULL mask specified or NULL callback given
 *  - -2 = specified mask was already present
 *
 * Any other value comes from ubus_register_event_handler, which I suppose / hope, returns positive values only.
 * I am not sure of that, but it seems an enum is consistently used in here.
*/
gint agh_ubus_event_add(struct agh_ubus_ctx *uctx, ubus_event_handler_t cb, const gchar *mask) {
	gint retval;
	gchar *mask_tmp;
	guint i;
	guint num_masks;

	retval = 0;

	if (!uctx || !mask || !cb) {
		agh_log_ubus_crit("no AGH ubus context, NULL mask specified or NULL callback given");
		retval = -1;
		goto wayout;
	}

	if (uctx->event_masks) {
		num_masks = g_queue_get_length(uctx->event_masks);
		for (i=0;i<num_masks;i++) {
			mask_tmp = g_queue_peek_nth(uctx->event_masks, i);
			if (!g_strcmp0(mask_tmp, mask)) {
				agh_log_ubus_dbg("mask already present");
				retval = -2;
				goto wayout;
			}

		}
	}

	uctx->event_handler->cb = cb;

	retval = ubus_register_event_handler(uctx->ctx, uctx->event_handler, mask);
	if (retval) {
		agh_log_ubus_dbg("ubus_register_event_handler returned a failure (code=%" G_GINT16_FORMAT")");
		uctx->event_handler->cb = NULL;
		goto wayout;
	}

	if (!uctx->event_masks)
		uctx->event_masks = g_queue_new();

	g_queue_push_tail(uctx->event_masks, g_strdup(mask));

wayout:
	return retval;
}

/*
 * This function "resets" the list of masks we're interested in being notified about.
 *
 * Returns: on success, an integer with value 0 is returned. A value of -10 is returned when the specified AGH ubus context is NULL, or no masks GQueue was found.
 * Any other error comes directly from ubus_unregister_event_handler, and should be positive, as they seem to consistently use an enum.
*/
gint agh_ubus_event_disable(struct agh_ubus_ctx *uctx) {
	gint retval;

	retval = -10;

	if (!uctx || !uctx->event_masks) {
		agh_log_ubus_crit("AGH ubus context or uctx->event_masks where NULL");
		return retval;
	}

	if ( (retval = ubus_unregister_event_handler(uctx->ctx, uctx->event_handler)) ) {
		agh_log_ubus_crit("ubus_unregister_event_handler returned a failure (code=%" G_GINT16_FORMAT")");
		return retval;
	}

	g_queue_free_full(uctx->event_masks, g_free);
	uctx->event_masks = NULL;
	uctx->event_handler->cb = NULL;

	return retval;
}

/*
 * This function performs ubus calls.
 * It relies on the ubus APIs, so it may return errors directly from them.
 * Call results are stored in the agh_ubus_call_data_str global pointer; the string to which it pointed before calling this function is freed.
 *
 * Some notes:
 * 1 - Should blob_buf_init fail, returning -ENOMEM.
 * 2 - At the end of this function, we check for bbuf not being NULL: we experienced segfaults otherwise.
 *     (I think it's because of blob_buf_free accessing buf->buf via free, but did not check.)
 *
 * Returns: an integer value with value 0 on success, or
 *  - AGH_UBUS_CALL_ERROR_BAD_ARGS (-80) when "path" or "method" parameters where NULL
 * - AGH_UBUS_CALL_ERROR_ALLOCFAILURE (-81) on memory allocation failure
 * - AGH_UBUS_CALL_ERROR_INVALID_JSON_MESSAGE (-82) when JSON message parsing failed.
 * Any other value should be considered as returned by other functions
 * (e.g.: -ENOMEM may be returned due to failure in blob_buf_init).
 * For all positive value errors, we recommend the usage of ubus_strerror.
*/
gint agh_ubus_call(struct agh_ubus_ctx *uctx, const gchar *path, const gchar *method, const gchar *message) {
	struct blob_buf *bbuf;
	guint32 id;
	gint retval;

	id = 0;
	retval = 0;
	bbuf = NULL;

	g_free(agh_ubus_call_data_str);
	agh_ubus_call_data_str = NULL;

	if (!path || !method) {
		agh_log_ubus_dbg("path or method are NULL, and this is not legal");
		retval = AGH_UBUS_CALL_ERROR_BAD_ARGS;
		goto wayout;
	}

	bbuf = g_try_malloc0(sizeof(*bbuf));
	if (!bbuf) {
		agh_log_ubus_crit("can not allocate a blob_buf structure");
		retval = AGH_UBUS_CALL_ERROR_ALLOCFAILURE;
		goto wayout;
	}

	if ( (retval = blob_buf_init(bbuf, 0)) ) {
		agh_log_ubus_crit("blob_buf init failure");
		goto wayout;
	}

	if (message && !blobmsg_add_json_from_string(bbuf, message)) {
		agh_log_ubus_dbg("failure while parsing JSON message");
		retval = AGH_UBUS_CALL_ERROR_INVALID_JSON_MESSAGE;
		goto wayout;
	}

	if ( (retval = ubus_lookup_id(uctx->ctx, path, &id)) ) {
		agh_log_ubus_dbg("failure from ubus_lookup_id");
		goto wayout;
	}

	retval = ubus_invoke(uctx->ctx, id, method, bbuf->head, agh_receive_call_result_data, NULL, 80 * 1000);

wayout:

	if (bbuf) {
		blob_buf_free(bbuf);
		g_free(bbuf);
	}

	return retval;
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
	else {
		agh_log_ubus_dbg("no data to return");
		agh_log_ubus_dbg("caller asked us to return global pointer");
	}

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
