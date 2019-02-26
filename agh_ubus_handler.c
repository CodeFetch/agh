#include <libubox/blobmsg_json.h>
#include "agh_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_ubus.h"
#include "agh_ubus_handler.h"
#include "agh_ubus_helpers.h"
#include "agh_ubus_logstream.h"

/* Functions prototypes. */
static void agh_ubus_handler_list(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd);
static void agh_ubus_handler_call(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd);
static void agh_ubus_handler_listen(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd);
static void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg);
static void agh_ubus_handler_logstream(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd);

gpointer agh_core_ubus_cmd_handle(gpointer data, gpointer hmessage) {
	struct agh_handler *h = data;
	struct agh_state *mstate = h->handler_data;
	struct agh_message *m = hmessage;

	struct agh_cmd *cmd;
	struct agh_ubus_ctx *uctx;
	struct agh_message *answer;
	void (*agh_ubus_cmd_handler_cb)(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd);
	config_setting_t *arg;
	const gchar *argstr;

	cmd = NULL;
	uctx = mstate->uctx;
	answer = NULL;
	agh_ubus_cmd_handler_cb = NULL;
	arg = NULL;
	argstr = NULL;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	if (!mstate->uctx)
		return NULL;

	cmd = m->csp;

	if (g_strcmp0(agh_cmd_get_operation(cmd), AGH_CMD_UBUS))
		return NULL;

	agh_cmd_answer_alloc(cmd);

	if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_NO_CONNECTION, TRUE);
		answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	arg = agh_cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);
	if (!arg) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_SUBCOMMAND, TRUE);
		answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	argstr = config_setting_get_string(arg);
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LIST))
		agh_ubus_cmd_handler_cb = agh_ubus_handler_list;
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_CALL))
		agh_ubus_cmd_handler_cb = agh_ubus_handler_call;
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LISTEN))
		agh_ubus_cmd_handler_cb = agh_ubus_handler_listen;
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LOGSTREAM))
		agh_ubus_cmd_handler_cb = agh_ubus_handler_logstream;

	if (!agh_ubus_cmd_handler_cb) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_INVALID_SUBCOMMAND, TRUE);
	} else {
		agh_ubus_cmd_handler_cb(uctx, cmd);
	}

	answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);
	return answer;
}

/*
 * You may think, as I did, that this is actually an asynchronous function, and that agh_ubus_list_receive_results is the callback invoked when operation is completed.
 * Actually, it seems the operation is synchronous, or at least this is what running this code showed. This is why it made sense to me to implement, and use, the agh_cmd_answer_if_empty function.
*/
static void agh_ubus_handler_list(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd) {
	const gchar *path;
	config_setting_t *arg;

	path = NULL;
	arg = NULL;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);

	ubus_lookup(uctx->ctx, path, agh_ubus_handler_list_receive_results, cmd);

	agh_cmd_answer_if_empty(cmd, AGH_CMD_ANSWER_STATUS_FAIL, AGH_UBUS_HANDLER_NO_DATA, FALSE);

	return;
}

static void agh_ubus_handler_call(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd) {
	const gchar *path;
	config_setting_t *arg;
	const gchar *method;
	const gchar *message;
	gint status;
	gchar *res;

	path = NULL;
	method = NULL;
	message = NULL;
	arg = NULL;
	status = 0;
	res = NULL;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);

	arg = agh_cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

	if (arg)
		method = config_setting_get_string(arg);

	arg = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);

	if (arg)
		message = config_setting_get_string(arg);

	status = agh_ubus_call(uctx, path, method, message);

	if (status) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);

		switch(status) {
			case AGH_UBUS_CALL_MISSING_PATH:
				agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_PATH, TRUE);
				break;
			case AGH_UBUS_CALL_MISSING_METHOD:
				agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_METHOD, TRUE);
				break;
			case AGH_UBUS_CALL_BLOB_BUF_INIT_FAILURE:
				agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_BBUF_INIT_ERROR, TRUE);
				break;
			case AGH_UBUS_CALL_INVALID_JSON_MESSAGE:
				agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_INVALID_JSON_MESSAGE, TRUE);
				break;
			case AGH_UBUS_CALL_METHOD_NOT_FOUND:
				agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_METHOD_NOT_FOUND, TRUE);
				break;
		}
	}
	else {
		res = agh_ubus_get_call_result(TRUE);
		if (res) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_set_data(cmd, TRUE);
			agh_cmd_answer_addtext(cmd, g_strdup_printf("\n%s", res), FALSE);
			g_free(res);
		}
	}

	agh_cmd_answer_if_empty(cmd, AGH_CMD_ANSWER_STATUS_FAIL, AGH_UBUS_HANDLER_NO_DATA, FALSE);

	return;
}

static void agh_ubus_handler_listen(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd) {
	config_setting_t *arg;
	const gchar *arg_str;
	guint i;
	guint num_events;
	const gchar *current_event_mask;

	arg = NULL;
	arg_str = NULL;
	i = 0;
	num_events = 0;
	current_event_mask = NULL;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	/* If no subcommand, then show current status. */
	if (!arg) {
		if (!uctx->event_masks) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_NOT_ENABLED, TRUE);
			return;
		}

		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		num_events = g_queue_get_length(uctx->event_masks);

		for (i=0;i<num_events;i++) {
			current_event_mask = g_queue_peek_nth(uctx->event_masks, i);
			agh_cmd_answer_addtext(cmd, current_event_mask, TRUE);
		}

		return;
	}

	arg_str = config_setting_get_string(arg);

	/* Or maybe we want to add anew event mask? */
	if (!g_strcmp0(arg_str, AGH_CMD_UBUS_LISTEN_ADD)) {

		arg = agh_cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

		if (!arg)
			current_event_mask = "*";
		else
			current_event_mask = config_setting_get_string(arg);
		
		if (agh_ubus_event_add(uctx, agh_ubus_handler_receive_event, current_event_mask)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_FAILED, TRUE);
			return;
		}
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_OK, TRUE);
		return;
	}

	/* Or disable events reporting. */
	if (!g_strcmp0(arg_str, AGH_CMD_UBUS_LISTEN_STOP)) {
		if (agh_ubus_event_disable(uctx)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_FAILED, TRUE);
			return;
		}
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_OK, TRUE);
		return;
	}

	agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
	agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_UNKNOWN_SUBCOMMAND, TRUE);
	return;
}

static void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg) {
	struct agh_cmd *agh_event;
	gchar *event_message;

	agh_event = agh_cmd_event_alloc(NULL);
	event_message = NULL;

	event_message = blobmsg_format_json(msg, true);

	agh_cmd_answer_set_data(agh_event, TRUE);
	agh_cmd_answer_set_status(agh_event, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(agh_event, "\""AGH_UBUS_HANDLER_UBUS_EVENTs_NAME"\"", TRUE);
	agh_cmd_answer_addtext(agh_event, g_strdup_printf("\n{ \"%s\": %s }\n", type, event_message), FALSE);
	agh_cmd_emit_event(agh_ubus_aghcomm, agh_event);
	g_free(event_message);

	return;
}

static void agh_ubus_handler_logstream(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd) {
	config_setting_t *arg;
	const gchar *argstr;
	gint logstream_ret;

	arg = NULL;
	logstream_ret = -1;
	argstr = NULL;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (!arg) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_MISSING_SUBCOMMAND, TRUE);
		return;
	}

	argstr = config_setting_get_string(arg);

	/* Enable logstream. */
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LOGSTREAM_ACTIVATE)) {
		logstream_ret = agh_ubus_logstream_init(uctx);
	}

	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LOGSTREAM_DEACTIVATE)) {
		logstream_ret = agh_ubus_logstream_deinit(uctx);
	}

	switch(logstream_ret) {
		case 0:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_OK, TRUE);
			break;
		case 1:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INTERNAL_ERROR, TRUE);
			break;
		case 2:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_ACTIVE, TRUE);
			break;
		case 4:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INTERNAL_ERROR, TRUE);
			break;
		case 5:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_DEACTIVATED, TRUE);
			break;
		default:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			agh_cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INVALID_SUBCOMMAND, TRUE);
	}

	return;
}
