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
	struct handler *h = data;
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

	if (g_strcmp0(cmd_get_operation(cmd), AGH_CMD_UBUS))
		return NULL;

	cmd_answer_prepare(cmd);

	if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_NO_CONNECTION);
		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);
	if (!arg) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_SUBCOMMAND);
		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
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
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_INVALID_SUBCOMMAND);
	} else {
		agh_ubus_cmd_handler_cb(uctx, cmd);
	}

	answer = cmd_answer_msg(cmd, mstate->comm, NULL);
	return answer;
}

/*
 * You may think, as I did, that this is actually an asynchronous function, and that agh_ubus_list_receive_results is the callback invoked when operation is completed.
 * Actually, it seems the operation is synchronous, or at least this is what running this code showed. This is why it made sense to me to implement, and use, the cmd_answer_if_empty function.
*/
static void agh_ubus_handler_list(struct agh_ubus_ctx *uctx, struct agh_cmd *cmd) {
	const gchar *path;
	config_setting_t *arg;

	path = NULL;
	arg = NULL;

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);

	ubus_lookup(uctx->ctx, path, agh_ubus_handler_list_receive_results, cmd);

	cmd_answer_if_empty(cmd, AGH_CMD_ANSWER_STATUS_FAIL, AGH_UBUS_HANDLER_NO_DATA, FALSE);

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

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);

	arg = cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

	if (arg)
		method = config_setting_get_string(arg);

	arg = cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);

	if (arg)
		message = config_setting_get_string(arg);

	status = agh_ubus_call(uctx, path, method, message);

	if (status) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);

		switch(status) {
			case AGH_UBUS_CALL_MISSING_PATH:
				cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_PATH);
				break;
			case AGH_UBUS_CALL_MISSING_METHOD:
				cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_MISSING_METHOD);
				break;
			case AGH_UBUS_CALL_BLOB_BUF_INIT_FAILURE:
				cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_BBUF_INIT_ERROR);
				break;
			case AGH_UBUS_CALL_INVALID_JSON_MESSAGE:
				cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_INVALID_JSON_MESSAGE);
				break;
			case AGH_UBUS_CALL_METHOD_NOT_FOUND:
				cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_METHOD_NOT_FOUND);
				break;
		}
	}
	else {
		res = agh_ubus_get_call_result();
		if (res) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			cmd_answer_set_data(cmd, TRUE);
			cmd_answer_peektext(cmd, g_strdup_printf("\n%s", res));
			g_free(res);
		}
	}

	cmd_answer_if_empty(cmd, AGH_CMD_ANSWER_STATUS_FAIL, AGH_UBUS_HANDLER_NO_DATA, FALSE);

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

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	/* If no subcommand, then show current status. */
	if (!arg) {
		if (!uctx->event_masks) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_NOT_ENABLED);
			return;
		}

		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		num_events = g_queue_get_length(uctx->event_masks);

		for (i=0;i<num_events;i++) {
			current_event_mask = g_queue_peek_nth(uctx->event_masks, i);
			cmd_answer_addtext(cmd, current_event_mask);
		}

		return;
	}

	arg_str = config_setting_get_string(arg);

	/* Or maybe we want to add anew event mask? */
	if (!g_strcmp0(arg_str, AGH_CMD_UBUS_LISTEN_ADD)) {

		arg = cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

		if (!arg)
			current_event_mask = "*";
		else
			current_event_mask = config_setting_get_string(arg);
		
		if (agh_ubus_event_add(uctx, agh_ubus_handler_receive_event, current_event_mask)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_FAILED);
			return;
		}
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_OK);
		return;
	}

	/* Or disable events reporting. */
	if (!g_strcmp0(arg_str, AGH_CMD_UBUS_LISTEN_STOP)) {
		if (agh_ubus_event_disable(uctx)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_FAILED);
			return;
		}
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_OK);
		return;
	}

	agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
	cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_UNKNOWN_SUBCOMMAND);
	return;
}

static void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg) {
	struct agh_cmd *agh_event;
	gchar *event_message;

	agh_event = cmd_event_prepare();
	event_message = NULL;

	event_message = blobmsg_format_json(msg, true);

	cmd_answer_set_data(agh_event, TRUE);
	agh_cmd_answer_set_status(agh_event, AGH_CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(agh_event, "\""AGH_UBUS_HANDLER_UBUS_EVENTs_NAME"\"");
	cmd_answer_peektext(agh_event, g_strdup_printf("\n{ \"%s\": %s }\n", type, event_message));
	cmd_emit_event(agh_ubus_aghcomm, agh_event);
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

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (!arg) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_MISSING_SUBCOMMAND);
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
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_OK);
			break;
		case 1:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INTERNAL_ERROR);
			break;
		case 2:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_ACTIVE);
			break;
		case 4:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INTERNAL_ERROR);
			break;
		case 5:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_DEACTIVATED);
			break;
		default:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_LOGSTREAM_INVALID_SUBCOMMAND);
	}

	return;
}
