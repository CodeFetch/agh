#include "agh_ubus_logstream.h"
#include "agh_ubus_helpers.h"
#include "agh_commands.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_UBUS_LOGSTREAM domain. */
#define AGH_LOG_DOMAIN_UBUS_LOGSTREAM "LOGSTREAM"

/* Logging macros. */
#define agh_log_ubus_logstream_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_UBUS_LOGSTREAM, message, ##__VA_ARGS__)
#define agh_log_ubus_logstream_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_UBUS_LOGSTREAM, message, ##__VA_ARGS__)

/* logstream states: */
#define AGH_UBUS_LOGSTREAM_STATE_INIT 0
#define AGH_UBUS_LOGSTREAM_STATE_CHANNEL_INIT 1
#define AGH_UBUS_LOGSTREAM_STATE_CONNECTED 2
#define AGH_UBUS_LOGSTREAM_STATE_RECONNECT 3

/*
 * This function should be invoked when we have the fd we can use to communicate with logd.
 *
 * Returns: <nothing>.
*/
static void agh_ubus_logstream_fd_cb(struct ubus_request *req, int fd) {
	struct agh_ubus_logstream_ctx *lctx = req->priv;
	lctx->fd = fd;
	lctx->logstream_state++;
	return;
}

/*
 * Deinitializes logstream channel, and in particular the GIOChannel and the GSource used to "watch" it.
 *
 * Returns: an integer with value 0 on success, 10 when the supplied logstream context was NULL, 11 when there is no channel context.
*/
static gint agh_ubus_logstream_channel_deinit(struct agh_ubus_logstream_ctx *lctx) {

	if (!lctx) {
		agh_log_ubus_logstream_crit("NULL logstream context");
		return 10;
	}

	if (!lctx->logstream_channel) {
		agh_log_ubus_logstream_dbg("channel context not present");
		return 11;
	}

	if (lctx->logwatcher) {
		agh_log_ubus_logstream_dbg("deactivating logwatcher GSource");
		g_source_destroy(lctx->logwatcher);
		lctx->logwatcher = NULL;
		lctx->logwatcher_id = 0;
	}

	if (lctx->logstream_channel) {
		agh_log_ubus_logstream_dbg("unreferencing channel");
		g_io_channel_unref(lctx->logstream_channel);
		lctx->logstream_channel = NULL;
		lctx->logstream_channel_tag = 0;
	}

	lctx->fd = -1;

	if (lctx->cmsg) {
		g_string_free(lctx->cmsg, TRUE);
		lctx->cmsg = NULL;
	}

	return 0;
}

/*
 * I simulated problems and tested error paths. Do you see any problem here?
 *
 * This function is executed each time there is IO to process on the GIOChannel. It assembles system log messages, and emits events for them.
 * This function runs multiple times per log message, hence a 0 return value does not indicate "success" in the usual sense of the term.
 *
 * Returns: an integer with values as follows:
 *  - 0: maybe we are still processing a message, maybe not, but we are OK
 *  - 1: failure while allocating a message part
 *  - 2: GIOChannel was in a status different than G_IO_STATUS_NORMAL, and this is a problem :)
 *  - 3: message parse failed, failure in agh_ubus_logstream_parse_log
 *  - 4: event emission failure
 *
 * This function may lead to an unclean program termination.
 * Furthermore, even in the face of errors, the function may be invoked multiple times. (and this comment needs to be more precise)
*/
static gint agh_ubus_logstream_incoming_message(struct agh_ubus_logstream_ctx *lctx) {
	struct blob_attr *message_part;
	GIOStatus status;
	gsize data_read;
	gchar *parsed_text_log_message;
	struct agh_cmd *log_event;
	gint event_error_value;

	data_read = 0;
	parsed_text_log_message = NULL;  /* this is needed, or agh_ubus_logstream_parse_log will fail */
	event_error_value = 0;

	if (!lctx->cmsg)
		lctx->cmsg = g_string_new(NULL);

	while(lctx->cmsg) {
		message_part = g_try_malloc0(sizeof(*message_part));
		if (!message_part) {
			agh_log_ubus_logstream_crit("unable to allocate log message part");
			g_string_free(lctx->cmsg, TRUE);
			lctx->cmsg = NULL;
			return 1;
		}

		status = g_io_channel_read_chars(lctx->logstream_channel, (gchar *)message_part, sizeof(*message_part), &data_read, &lctx->gerr);
		if (status != G_IO_STATUS_NORMAL) {
			agh_log_ubus_logstream_crit("error while reading log channel data; ", lctx->gerr ? lctx->gerr->message : "unknown error");
			g_error_free(lctx->gerr);
			lctx->gerr = NULL;
			g_string_free(lctx->cmsg, TRUE);
			lctx->cmsg = NULL;
			g_free(message_part);
			return 2;
		}

		g_string_append_len(lctx->cmsg, (gchar*)message_part, sizeof(*message_part));
		g_free(message_part);

		/*
		 * We need a complete blob_attr structure to be able then to "safely" read the blob length via blob_len, in a successive iteration.
		*/
		if (lctx->cmsg->len < sizeof(struct blob_attr))
			break;

		if (lctx->cmsg->len < blob_len((struct blob_attr *)lctx->cmsg->str) + sizeof(struct blob_attr))
			break;

		/* We should have a complete message now. */
		message_part = (struct blob_attr *)g_string_free(lctx->cmsg, FALSE);
		lctx->cmsg = NULL;

		/* Don't get confused by the variable name, we are parsing the complete message now. */
		if (agh_ubus_logstream_parse_log(message_part, &parsed_text_log_message)) {
			agh_log_ubus_logstream_crit("message parsing failed; message is lost");
			g_free(message_part);
			return 3;
		}

		g_free(message_part);

		log_event = agh_cmd_event_alloc(&event_error_value);
		if (!log_event) {
			agh_log_ubus_logstream_crit("discarding logstream event due to agh_cmd_event_alloc failure (code=%" G_GINT16_FORMAT")", event_error_value);
			g_free(parsed_text_log_message);
			return 4;
		}

		agh_cmd_answer_set_data(log_event, TRUE);
		agh_cmd_answer_set_status(log_event, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(log_event, "\""AGH_UBUS_LOGSTREAM_LOG_EVENTs_NAME"\"", TRUE);
		agh_cmd_answer_addtext(log_event, parsed_text_log_message, FALSE);
		agh_cmd_emit_event(agh_ubus_aghcomm, log_event);

	}

	return 0;
}

/*
 * This is the channel IO watch function ("logwatcher"). As a GSource-derived type of function, it respects related GLib semantics.
 * Whenever agh_ubus_logstream_incoming_message fails, this function does emit a log message and "restarts" the GIOChannel, by setting lctx->logstream_state to AGH_UBUS_LOGSTREAM_STATE_RECONNECT
 *  (see agh_ubus_logstream_statemachine).
*/
static gboolean agh_ubus_logstream_channel_io(GIOChannel *channel, GIOCondition condition, gpointer data) {
	struct agh_ubus_logstream_ctx *lctx = data;
	gint retval;

	retval = 0;

	switch(condition) {
		case G_IO_IN:
		case G_IO_PRI:
			if ( (retval = agh_ubus_logstream_incoming_message(lctx)) ) {
				agh_log_ubus_logstream_crit("failure from agh_ubus_logstream_incoming_message (code=%" G_GINT16_FORMAT")", retval);
				lctx->logstream_state = AGH_UBUS_LOGSTREAM_STATE_RECONNECT;
			}
			break;
		case G_IO_ERR:
		case G_IO_HUP:
			lctx->logstream_state = AGH_UBUS_LOGSTREAM_STATE_RECONNECT;
			break;
		default:
			agh_log_ubus_logstream_crit("unknown GIOChannel condition");
			return FALSE;
	}

	return TRUE;
}

/*
 * Initializes a GLib GIOChannel to read messages from logd.
 * A watch GSource is also created, and attached to the supplied GMainContext.
 * This function may lead to an unclean program termination.
 *
 * Returns: an integer with value 0 on success, or
 *  - 20 when a NULL logstream context or GMainContext where given
 *  - 21 when a GIOChannel is already present
 *  - 22 when a failure occurred while setting channel encoding to NULL (to read binary data)
 *  - 23 when something gone wrong while attaching the watch to the GMainContext.
 *
 * Note: I don't understand well why I should use g_source_unref when attaching the watch to the given GMainContext fails. What about g_source_destroy?
*/
static gint agh_ubus_logstream_channel_init(struct agh_ubus_logstream_ctx *lctx, GMainContext *gmctx) {
	GIOStatus status;
	gint retval;

	retval = 0;

	if (!lctx || !gmctx) {
		agh_log_ubus_logstream_crit("NULL logstream context or GMainContext");
		retval = 20;
		goto wayout;
	}

	if (lctx->logstream_channel) {
		agh_log_ubus_logstream_crit("seems an IO channel is already present");
		retval = 21;
		goto wayout;
	}

	lctx->logstream_channel = g_io_channel_unix_new(lctx->fd);

	status = g_io_channel_set_encoding(lctx->logstream_channel, NULL, &lctx->gerr);

	if (status != G_IO_STATUS_NORMAL) {
		agh_log_ubus_logstream_crit("can not set NULL channel encoding for log channel",lctx->gerr ? lctx->gerr->message : "unknown error");
		g_error_free(lctx->gerr);
		lctx->gerr = NULL;
		agh_ubus_logstream_channel_deinit(lctx);
		retval = 22;
		goto wayout;
	}

	g_io_channel_set_close_on_unref(lctx->logstream_channel, TRUE);

	/*
	 * If we didn't care about what GMainContext we are going to attach this watch to, we could do:
	 * lctx->logstream_channel_tag = g_io_add_watch(lctx->logstream_channel, G_IO_IN, agh_ubus_logstream_channel_io, lctx);
	*/

	lctx->logwatcher = g_io_create_watch(lctx->logstream_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP);
	g_source_set_callback(lctx->logwatcher, G_SOURCE_FUNC(agh_ubus_logstream_channel_io), lctx, NULL);
	lctx->logwatcher_id = g_source_attach(lctx->logwatcher, gmctx);
	g_source_unref(lctx->logwatcher);
	if (!lctx->logwatcher_id) {
		agh_log_ubus_logstream_crit("failed to attach logwatcher to this GmainContext");
		lctx->logwatcher = NULL;
		agh_ubus_logstream_channel_deinit(lctx);
		retval = 23;
		goto wayout;
	}

	agh_log_ubus_logstream_dbg("channel *SEEMS* OK");

wayout:
	return retval;
}

/*
 * The is the logstream state machine: implemented as a GSOurce, it checks at any time whether we are connected to logd or not.
 * Should we get disconnected for any reason, this GSource will try to reconnect. I am pretty certain we can do this more efficiently.
 * Note how in this function we set to NULL every pointer on lctx (which is an agh_ubus_logstream_ctx struct) after freeing the related memory.
 * Infact, calling g_free (or free) on a NULL ptr is fine, while calling it on already freed memory is not.
 *
 * States:
 * AGH_UBUS_LOGSTREAM_STATE_INIT (0): when in this state, we try to get a file descriptor to receive messages from ubox's logd.
 * Thus, we send a request to logd itself via ubus, retrying indefinitely until we get a response, or we are removed/destroyed.
 * If we're not connected to ubus, then our current request, and associated blob buffer, are deallocated, and we'll rebuild them next time we're connected to ubus again
 *  (there's no valid reason for that at the moment, I think it just seemed a goo idea at the time; read the code as this comment does not explain the concept adequately).
 * Only a single request will be made, regardless how many times we're run. We expect a state transition to happen via the agh_ubus_logstream_fd_cb function.
 *
 * AGH_UBUS_LOGSTREAM_STATE_CHANNEL_INIT (1): we got a file descriptor, so we init the logstream channel on it.
 *
 * AGH_UBUS_LOGSTREAM_STATE_CONNECTED (2): we're happy.
 *
 * AGH_UBUS_LOGSTREAM_STATE_RECONNECT (3): we deinitialize the logstream channel, and return to AGH_UBUS_LOGSTREAM_STATE_INIT.
*/
static gboolean agh_ubus_logstream_statemachine(gpointer data) {
	struct agh_ubus_ctx *uctx = data;
	struct agh_ubus_logstream_ctx *lctx = uctx->logstream_ctx;

	guint32 id;

	id = 0;

	switch(lctx->logstream_state) {
		case AGH_UBUS_LOGSTREAM_STATE_INIT:

			if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
				if (lctx->b) {
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
				}

				if (lctx->current_req) {
					g_free(lctx->current_req);
					lctx->current_req = NULL;
				}

				break;
			}

			if (!lctx->current_req) {
				agh_log_ubus_logstream_dbg("asking for FD");

				lctx->b = g_try_malloc0(sizeof(struct blob_buf));
				if (!lctx->b) {
					agh_log_ubus_logstream_dbg("failed to allocate struct blob_buf");
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				if (blob_buf_init(lctx->b, 0)) {
					agh_log_ubus_logstream_dbg("blob buf init failed");
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				if (blobmsg_add_u8(lctx->b, "stream", 1)) {
					agh_log_ubus_logstream_dbg("failure while adding stream attribute");
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				if (blobmsg_add_u8(lctx->b, "oneshot", 0)) {
					agh_log_ubus_logstream_dbg("failure while adding oneshot attribute");
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				if (blobmsg_add_u32(lctx->b, "lines", 0)) {
					agh_log_ubus_logstream_dbg("failure while addint lines attribute");
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				if (ubus_lookup_id(uctx->ctx, "log", &id)) {
					agh_log_ubus_logstream_dbg("no log object in ubus");
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					break;  /* this is considered more like a temporary failure */
				}

				lctx->current_req = g_try_malloc0(sizeof(struct ubus_request));
				if (!lctx->current_req) {
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					lctx->logstream_reconnect = NULL;
					return FALSE;
				}

				ubus_invoke_async(uctx->ctx, id, "read", lctx->b->head, lctx->current_req);
				lctx->current_req->fd_cb = agh_ubus_logstream_fd_cb;
				lctx->current_req->priv = lctx;

				ubus_complete_request_async(uctx->ctx, lctx->current_req);
			}

			break;
		case AGH_UBUS_LOGSTREAM_STATE_CHANNEL_INIT:
			blob_buf_free(lctx->b);
			g_free(lctx->b);
			lctx->b = NULL;
			g_free(lctx->current_req);
			lctx->current_req = NULL;

			/*
			 * If the file descriptor is plain invalid, you may not face major issues, except for some rightly emitted GLib warnings. :)
			 * Still, I don't know what could happen if you get a wrong FD.
			*/
			if (lctx->fd<0) {
				agh_log_ubus_logstream_crit("bad FD?");
				lctx->logstream_reconnect = NULL;
				return FALSE;
			}
			agh_log_ubus_logstream_dbg("FD = %" G_GINT16_FORMAT"",lctx->fd);
			if (!agh_ubus_logstream_channel_init(lctx, uctx->gmctx))
				lctx->logstream_state++;
			else {
				agh_log_ubus_logstream_crit("failure while invoking agh_ubus_logstream_channel_init");
				lctx->logstream_reconnect = NULL;
				return FALSE;
			}

			break;
		case AGH_UBUS_LOGSTREAM_STATE_CONNECTED:
			break;
		case AGH_UBUS_LOGSTREAM_STATE_RECONNECT:
			agh_ubus_logstream_channel_deinit(lctx);
			lctx->logstream_state = AGH_UBUS_LOGSTREAM_STATE_INIT;
			break;
		default:
			agh_log_ubus_logstream_crit("unknown state");
			lctx->logstream_reconnect = NULL;
			return FALSE;
	}

	return TRUE;
}

/*
 * Initializes log streaming, by basically allocating needed context and attaching our timeout source (agh_ubus_logstream_statemachine)
 * to a GMainContext.
 *
 * Returns: an integer with value 0 on success, or
 *  - 1: no ubus context
 *  - 2: logstream context already allocated
 *  - 3: allocation failure
 *  - 4: failure while attaching our timeout source.
 *
 * This function may lead to an unclean program termination.
*/
gint agh_ubus_logstream_init(struct agh_ubus_ctx *uctx) {
	struct agh_ubus_logstream_ctx *lctx;
	gint retval;

	retval = 0;

	if (!uctx) {
		agh_log_ubus_logstream_crit("no AGH ubus context, not initializing");
		retval = 1;
		goto wayout;
	}

	if (uctx->logstream_ctx) {
		agh_log_ubus_logstream_crit("logstream context already allocated");
		retval = 2;
		goto wayout;
	}

	uctx->logstream_ctx = g_try_malloc0(sizeof(*(uctx->logstream_ctx)));
	if (!uctx->logstream_ctx) {
		agh_log_ubus_logstream_crit("unable to allocate logstream context");
		retval = 3;
		goto wayout;
	}

	lctx = uctx->logstream_ctx;

	lctx->logstream_reconnect = g_timeout_source_new(AGH_UBUS_LOGSTREAM_CHECK_INTERVAL);
	g_source_set_callback(lctx->logstream_reconnect, agh_ubus_logstream_statemachine, uctx, NULL);
	lctx->logstream_reconnect_tag = g_source_attach(lctx->logstream_reconnect, uctx->gmctx);
	g_source_unref(lctx->logstream_reconnect);
	lctx->fd = -1;
	if (!lctx->logstream_reconnect_tag) {
		agh_log_ubus_logstream_crit("failed to attach logstream timeout source to GMainContext");
		lctx->logstream_reconnect = NULL;
		retval = 4;
		goto wayout;
	}

wayout:
	return retval;
}

/*
 * This function deinitializes logstream, and its log channel.
 *
 * Returns: an integer with value 0 on success, or
 *  - 4 = no agh ubus context was present
 *  - 5 = no logstream context
*/
gint agh_ubus_logstream_deinit(struct agh_ubus_ctx *uctx) {
	struct agh_ubus_logstream_ctx *lctx;

	if (!uctx) {
		agh_log_ubus_logstream_crit("no AGH ubus context");
		return 4;
	}

	if (!uctx->logstream_ctx) {
		agh_log_ubus_logstream_crit("no logstream context");
		return 5;
	}

	lctx = uctx->logstream_ctx;

	if (lctx->logstream_reconnect) {
		g_source_destroy(lctx->logstream_reconnect);
		lctx->logstream_reconnect = NULL;
		lctx->logstream_reconnect_tag = 0;
	}

	if (lctx->current_req) {
		g_free(lctx->current_req);
		lctx->current_req = NULL;
	}

	if (lctx->b) {
		blob_buf_free(lctx->b);
		g_free(lctx->b);
		lctx->b = NULL;
	}

	agh_ubus_logstream_channel_deinit(lctx);

	g_free(uctx->logstream_ctx);
	uctx->logstream_ctx = NULL;

	return 0;
}
