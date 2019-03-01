#include <libubox/blobmsg_json.h>
#include "agh_handlers.h"
#include "agh_logging.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_ubus.h"
#include "agh_ubus_handler.h"
#include "agh_ubus_helpers.h"
#include "agh_ubus_logstream.h"

/* Log messages from AGH_LOG_DOMAIN_UBUS_HANDLER domain. */
#define AGH_LOG_DOMAIN_UBUS_HANDLER	"UBUS_HANDLER"

/* Logging macros. */
#define agh_log_ubus_handler_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_UBUS_HANDLER, message, ##__VA_ARGS__)
#define agh_log_ubus_handler_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_UBUS_HANDLER_CRIT, message, ##__VA_ARGS__)

/*
 * Lists ubus objects and related methods.
 *
 * Returns: always 0.
*/
static gint agh_ubus_cmd_list_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	config_setting_t *arg;
	const gchar *path;
	gint ubus_retval;

	path = NULL;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);
	if (arg)
		path = config_setting_get_string(arg);

	ubus_retval = ubus_lookup(mstate->uctx->ctx, path, agh_ubus_handler_list_receive_results, cmd);

	if (ubus_retval)
		agh_cmd_answer_addtext(cmd, ubus_strerror(ubus_retval), TRUE);

	return 0;
}

static const struct agh_cmd_operation agh_ubus_handler_subcommands[] = {
	{
		.op_name = AGH_CMD_UBUS_LIST,
		.min_args = 0,
		.max_args = 1,
		.cmd_cb = agh_ubus_cmd_list_cb
	},

	{ }
};

gint agh_ubus_cmd_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;

	retval = 0;

	if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
		agh_cmd_op_answer_error(cmd, AGH_CMD_ANSWER_STATUS_FAIL, "NO_CONNECTION", TRUE);
		retval = 100;
		goto wayout;
	}

	agh_cmd_op_match(mstate, agh_ubus_handler_subcommands, cmd, 1);

wayout:
	return retval;
}

/* ubus operations */
static const struct agh_cmd_operation agh_ubus_handler_ops[] = {
	{
		.op_name = AGH_CMD_UBUS,
		.min_args = 1,
		.max_args = 4,
		.cmd_cb = agh_ubus_cmd_cb
	},

	{ }
};

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

		agh_cmd_answer_addtext(cmd, ubus_strerror(status), TRUE);

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


gpointer agh_core_ubus_cmd_handle(gpointer data, gpointer hmessage) {
	struct agh_handler *h = data;
	struct agh_state *mstate = h->handler_data;
	struct agh_message *m = hmessage;
	struct agh_cmd *cmd;
	struct agh_message *answer;

	cmd = NULL;
	answer = NULL;

	if ((m->msg_type != MSG_SENDCMD) || (!mstate->uctx))
		goto wayout;

	cmd = m->csp;
	agh_cmd_op_match(mstate, agh_ubus_handler_ops, cmd, 0);

wayout:
	answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);
	return answer;
}
