#include "agh_ubus_logstream.h"
#include "agh_ubus_helpers.h"
#include "agh_commands.h"

static gboolean agh_ubus_logstream_statemachine(gpointer data);
static void agh_ubus_logstream_fd_cb(struct ubus_request *req, int fd);
static void agh_ubus_logstream_channel_init(struct agh_ubus_logstream_ctx *lctx, GMainContext *gmctx);
static void agh_ubus_logstream_channel_deinit(struct agh_ubus_logstream_ctx *lctx);
static gboolean agh_ubus_logstream_channel_io(GIOChannel *channel, GIOCondition condition, gpointer data);
static void agh_ubus_logstream_incoming_message(struct agh_ubus_logstream_ctx *lctx);

gint agh_ubus_logstream_init(struct agh_ubus_ctx *uctx) {
	struct agh_ubus_logstream_ctx *lctx;

	lctx = NULL;

	if (!uctx)
		return 1;

	if (uctx->logstream_ctx)
		return 2;

	uctx->logstream_ctx = g_malloc0(sizeof(struct agh_ubus_logstream_ctx));
	lctx = uctx->logstream_ctx;

	lctx->logstream_reconnect = g_timeout_source_new(AGH_UBUS_LOGSTREAM_CHECK_INTERVAL);
	g_source_set_callback(lctx->logstream_reconnect, agh_ubus_logstream_statemachine, uctx, NULL);
	lctx->logstream_reconnect_tag = g_source_attach(lctx->logstream_reconnect, uctx->gmctx);
	g_source_unref(lctx->logstream_reconnect);
	lctx->fd = -1;

	return 0;
}

gint agh_ubus_logstream_deinit(struct agh_ubus_ctx *uctx) {
	struct agh_ubus_logstream_ctx *lctx;

	lctx = NULL;

	if (!uctx)
		return 4;

	if (!uctx->logstream_ctx)
		return 5;

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
	lctx = NULL;

	return 0;
}

static gboolean agh_ubus_logstream_statemachine(gpointer data) {
	struct agh_ubus_ctx *uctx = data;
	struct agh_ubus_logstream_ctx *lctx = uctx->logstream_ctx;

	guint32 id;

	id = 0;

	switch(lctx->logstream_state) {
		case 0:

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
				g_print("%s: asking for FD\n",__FUNCTION__);

				lctx->b = g_malloc0(sizeof(struct blob_buf));
				if (blob_buf_init(lctx->b, 0)) {
					g_print("%s: blob buf init failed\n",__FUNCTION__);
					g_free(lctx->b);
					lctx->logstream_reconnect = NULL;
					lctx->b = NULL;
					return FALSE;
				}

				blobmsg_add_u8(lctx->b, "stream", 1);
				blobmsg_add_u8(lctx->b, "oneshot", 0);
				blobmsg_add_u32(lctx->b, "lines", 0);

				if (ubus_lookup_id(uctx->ctx, "log", &id)) {
					g_print("%s: no log object in ubus\n",__FUNCTION__);
					blob_buf_free(lctx->b);
					g_free(lctx->b);
					lctx->b = NULL;
					break;
				}

				lctx->current_req = g_malloc0(sizeof(struct ubus_request));
				ubus_invoke_async(uctx->ctx, id, "read", lctx->b->head, lctx->current_req);
				lctx->current_req->fd_cb = agh_ubus_logstream_fd_cb;
				lctx->current_req->priv = lctx;
				ubus_complete_request_async(uctx->ctx, lctx->current_req);
			}

			break;
		case 1:
			blob_buf_free(lctx->b);
			g_free(lctx->b);
			lctx->b = NULL;
			g_free(lctx->current_req);
			lctx->current_req = NULL;

			g_print("%s: FD = %" G_GINT16_FORMAT"\n",__FUNCTION__,lctx->fd);
			agh_ubus_logstream_channel_init(lctx, uctx->gmctx);
			lctx->logstream_state++;

			break;
		case 2:
			break;
		case 3:
			agh_ubus_logstream_channel_deinit(lctx);
			lctx->logstream_state = 0;
			break;
		default:
			g_print("%s: unknown state\n",__FUNCTION__);
			lctx->logstream_reconnect = NULL;
			return FALSE;
	}

	return TRUE;
}

static void agh_ubus_logstream_channel_init(struct agh_ubus_logstream_ctx *lctx, GMainContext *gmctx) {
	GIOStatus status;

	if (!lctx)
		return;

	if (lctx->logstream_channel)
		return;

	lctx->logstream_channel = g_io_channel_unix_new(lctx->fd);

	status = g_io_channel_set_encoding(lctx->logstream_channel, NULL, &lctx->gerr);

	g_io_channel_set_close_on_unref(lctx->logstream_channel, TRUE);

	if (status != G_IO_STATUS_NORMAL) {
		g_print("%s: can not set NULL channel encoding for log channel; %s\n",__FUNCTION__, lctx->gerr ? lctx->gerr->message : "unknown error");
		g_error_free(lctx->gerr);
		lctx->gerr = NULL;
		agh_ubus_logstream_channel_deinit(lctx);
		return;
	}

	g_print("%s: channel seems OK\n",__FUNCTION__);

	//lctx->logstream_channel_tag = g_io_add_watch(lctx->logstream_channel, G_IO_IN, agh_ubus_logstream_channel_io, lctx);

	lctx->logwatcher = g_io_create_watch(lctx->logstream_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP);
	/*
	 * When GLib 2.58 will be out, you will be able to suppress the warning happening here as follows:
	 * g_source_set_callback(lctx->logwatcher, G_SOURCE_FUNC(agh_ubus_logstream_channel_io), lctx, NULL);
	*/
	g_source_set_callback(lctx->logwatcher, ((GSourceFunc) (void (*)(void)) (agh_ubus_logstream_channel_io)), lctx, NULL);
	lctx->logwatcher_id = g_source_attach(lctx->logwatcher, gmctx);
	g_source_unref(lctx->logwatcher);

	return;
}

static void agh_ubus_logstream_channel_deinit(struct agh_ubus_logstream_ctx *lctx) {

	if (!lctx)
		return;

	if (!lctx->logstream_channel)
		return;

	if (lctx->logwatcher) {
		g_source_destroy(lctx->logwatcher);
		lctx->logwatcher = NULL;
		g_print("%s: deactivating logwatcher\n",__FUNCTION__);
	}

	if (lctx->logstream_channel) {
		g_io_channel_unref(lctx->logstream_channel);
		lctx->logstream_channel = NULL;
		lctx->logstream_channel_tag = 0;
	}

	lctx->fd = -1;

	if (lctx->cmsg) {
		g_string_free(lctx->cmsg, TRUE);
		lctx->cmsg = NULL;
	}

	return;
}

static void agh_ubus_logstream_fd_cb(struct ubus_request *req, int fd) {
	struct agh_ubus_logstream_ctx *lctx = req->priv;
	lctx->fd = fd;
	lctx->logstream_state++;
	return;
}

static gboolean agh_ubus_logstream_channel_io(GIOChannel *channel, GIOCondition condition, gpointer data) {
	struct agh_ubus_logstream_ctx *lctx = data;

	switch(condition) {
		case G_IO_IN:
			agh_ubus_logstream_incoming_message(lctx);
			break;
		case G_IO_PRI:
			agh_ubus_logstream_incoming_message(lctx);
			break;
		case G_IO_ERR:
		case G_IO_HUP:
			lctx->logstream_state = 3;
			break;
		default:
			g_print("%s: unknown condition",__FUNCTION__);
			return FALSE;
	}

	return TRUE;
}

static void agh_ubus_logstream_incoming_message(struct agh_ubus_logstream_ctx *lctx) {
	struct blob_attr *message_part;
	GIOStatus status;
	gsize data_read;
	gchar *parsed_text_log_message;
	struct agh_cmd *log_event;

	message_part = NULL;
	data_read = 0;
	parsed_text_log_message = NULL;
	log_event = NULL;

	if (!lctx->cmsg)
		lctx->cmsg = g_string_new(NULL);

	while(lctx->cmsg) {
		message_part = g_malloc0(sizeof(struct blob_attr));
		status = g_io_channel_read_chars(lctx->logstream_channel, (gchar *)message_part, sizeof(struct blob_attr), &data_read, &lctx->gerr);
		if (status != G_IO_STATUS_NORMAL) {
			g_print("%s: error while reading log channel data; %s\n",__FUNCTION__, lctx->gerr ? lctx->gerr->message : "unknown error");
			g_error_free(lctx->gerr);
			lctx->gerr = NULL;
			g_string_free(lctx->cmsg, TRUE);
			lctx->cmsg = NULL;
			g_free(message_part);
			message_part = NULL;
			return;
		}

		g_string_append_len(lctx->cmsg, (gchar*)message_part, sizeof(struct blob_attr));
		g_free(message_part);

		if (lctx->cmsg->len < sizeof(struct blob_attr))
			break;

		if (lctx->cmsg->len < blob_len((struct blob_attr *)lctx->cmsg->str) + sizeof(struct blob_attr))
			break;

		message_part = (struct blob_attr *)g_string_free(lctx->cmsg, FALSE);
		lctx->cmsg = NULL;

		/* Don't get confused by the variable name, we are parsing the complete message now. */
		agh_ubus_logstream_parse_log(message_part, &parsed_text_log_message);

		g_free(message_part);

		log_event = cmd_event_prepare();
		cmd_answer_set_data(log_event, TRUE);
		agh_cmd_answer_set_status(log_event, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(log_event, "\""AGH_UBUS_LOGSTREAM_LOG_EVENTs_NAME"\"");
		cmd_answer_peektext(log_event, parsed_text_log_message);
		cmd_emit_event(agh_ubus_aghcomm, log_event);
		parsed_text_log_message = NULL;
		log_event = NULL;

	}

	return;
}
