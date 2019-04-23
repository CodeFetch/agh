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
#define agh_log_ubus_handler_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_UBUS_HANDLER, message, ##__VA_ARGS__)

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

/*
 * Performs ubus calls.
 *
 * Returns: 0 on success, 101 when mandatory path argument is missing.
*/
static gint agh_ubus_cmd_call_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint status;
	gint retval;

	config_setting_t *arg;

	const gchar *path;
	const gchar *method;
	const gchar *message;

	gchar *res;

	path = NULL;
	method = NULL;
	message = NULL;
	retval = 0;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);
	if (arg)
		path = config_setting_get_string(arg);
	else {
		agh_log_ubus_handler_dbg("mandatory path not specified");
		retval = 101;
		goto wayout;
	}

	arg = agh_cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);
	if (arg)
		method = config_setting_get_string(arg);

	arg = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);
	if (arg)
		message = config_setting_get_string(arg);

	status = agh_ubus_call(mstate->uctx, path, method, message);

	switch(status) {
		case AGH_UBUS_CALL_ERROR_BAD_ARGS:
			agh_cmd_answer_addtext(cmd, "BAD_ARGS", TRUE);
			break;
		case AGH_UBUS_CALL_ERROR_ALLOCFAILURE:
			agh_cmd_answer_addtext(cmd, "BBUF_MALLOC_FAILURE", TRUE);
			break;
		case AGH_UBUS_CALL_ERROR_INVALID_JSON_MESSAGE:
			agh_cmd_answer_addtext(cmd, "INVALID_JSON", TRUE);
			break;
		case -ENOMEM:
			agh_cmd_answer_addtext(cmd, "ENOMEM", TRUE);
			break;
		case UBUS_STATUS_OK:
			res = agh_ubus_get_call_result(TRUE);
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_set_data(cmd, TRUE);
			agh_cmd_answer_addtext(cmd, g_strdup_printf("\n%s", res), FALSE);
			g_free(res);
			break;
		default:
			agh_cmd_answer_addtext(cmd, ubus_strerror(status), TRUE);
			break;
	}

wayout:
	return retval;
}

/*
 * This function runs when an ubus event has been received.
 *
 * Returns: <nothing>.
*/
static void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg) {
	struct agh_cmd *agh_event;
	gchar *event_message;
	gint error_value;

	error_value = 0;

	if (!agh_ubus_aghcomm || agh_ubus_aghcomm->teardown_in_progress) {
		agh_log_ubus_handler_crit("discarding event due to missing agh_ubus_aghcomm, or agh_ubus_aghcomm teardown being in progress");
		return;
	}

	agh_event = agh_cmd_event_alloc(&error_value);
	if (!agh_event) {
		agh_log_ubus_handler_crit("discarding event due to agh_cmd_event_alloc failure (code=%" G_GINT16_FORMAT")", error_value);
		return;
	}

	event_message = blobmsg_format_json(msg, true);

	if (event_message) {
		agh_cmd_answer_set_data(agh_event, TRUE);
		agh_cmd_answer_set_status(agh_event, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(agh_event, "\""AGH_UBUS_HANDLER_UBUS_EVENTs_NAME"\"", TRUE);
		agh_cmd_answer_addtext(agh_event, g_strdup_printf("\n{ \"%s\": %s }\n", type, event_message), FALSE);
	}
	else {
		agh_cmd_answer_set_status(agh_event, AGH_CMD_ANSWER_STATUS_FAIL);
		agh_cmd_answer_addtext(agh_event, "UBUS_EVENT_LOST", TRUE);
	}

	agh_cmd_emit_event(agh_ubus_aghcomm, agh_event);

	g_free(event_message);

	return;
}

/*
 * Listen for new ubus events, and maintain an internal (to AGH) mask list.
 *
 * Returns: always 0;
*/
static gint agh_ubus_cmd_listen_add_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint ubus_retval;
	config_setting_t *arg;
	const gchar *current_mask;

	ubus_retval = 0;

	arg = agh_cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

	if (!arg)
		current_mask = "*";
	else
		current_mask = config_setting_get_string(arg);

	ubus_retval = agh_ubus_event_add(mstate->uctx, agh_ubus_handler_receive_event, current_mask);

	switch(ubus_retval) {
		case -2:
			agh_cmd_answer_addtext(cmd, "ALREADY_PRESENT", TRUE);
			break;
		case UBUS_STATUS_OK:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, "OK", TRUE);
			break;
		default:
			agh_cmd_answer_addtext(cmd, ubus_strerror(ubus_retval), TRUE);
			break;
	}

	return 0;
}

/*
 * Disable ubus events reporting.
 *
 * Returns: an integer with value 0 on success, or
 *  - 101: when no ubus context or event masks queue are found.
*/
static gint agh_ubus_cmd_listen_reset_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;
	gint ubus_retval;

	retval = 0;

	if (!mstate->uctx || !mstate->uctx->event_masks) {
		retval = 101;
		goto wayout;
	}

	ubus_retval = agh_ubus_event_disable(mstate->uctx);

	if (ubus_retval)
		agh_cmd_answer_addtext(cmd, ubus_strerror(ubus_retval), TRUE);
	else {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, "OK", TRUE);
	}

wayout:
	return retval;
}

/* AGH_CMD_UBUS_LISTEN subcommands struct */
static const struct agh_cmd_operation agh_ubus_handler_listen_subcommands[] = {
	{
		.op_name = AGH_CMD_UBUS_LISTEN_ADD,
		.min_args = 0,
		.max_args = 1,
		.cmd_cb = agh_ubus_cmd_listen_add_cb
	},
	{
		.op_name = AGH_CMD_UBUS_LISTEN_STOP,
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_ubus_cmd_listen_reset_cb
	},

	{ }
};

/*
 * Handle ubus events related commands. With no subcommands, gives back current status.
 *
 * Returns: an integer with value 0 on success, 101 when no event masks queue is present and no subcommand has been found.
*/
static gint agh_ubus_cmd_listen_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	config_setting_t *arg;
	guint i;
	guint num_events;
	const gchar *current_event_mask;
	gint retval;

	i = 0;
	retval = 0;

	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	/* If no subcommand, then show current status. */
	if (!arg) {

		if (!mstate->uctx->event_masks) {
			agh_cmd_answer_addtext(cmd, "EVENTS_NOT_ENABLED", TRUE);
			retval = 101;
			goto wayout;
		}

		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);

		num_events = g_queue_get_length(mstate->uctx->event_masks);

		for (i=0;i<num_events;i++) {
			current_event_mask = g_queue_peek_nth(mstate->uctx->event_masks, i);
			agh_cmd_answer_addtext(cmd, current_event_mask, TRUE);
		}

		goto wayout;

	}

	agh_cmd_op_match(mstate, agh_ubus_handler_listen_subcommands, cmd, 2);

wayout:
	return retval;
}

/*
 * Enables log streaming.
 *
 * Returns: an integer with value 0 on success, or 101 when ubus context was NULL.
*/
static gint agh_ubus_cmd_logstream_enable_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint logstream_ret;
	gint retval;

	retval = 0;

	/* We should not be there if this is true... */
	if (!mstate->uctx) {
		agh_log_ubus_handler_crit("something apparently impossible happened while enabling logstream: mstate->uctx was NULL!");
		retval = 101;
		goto wayout;
	}

	logstream_ret = agh_ubus_logstream_init(mstate->uctx);

	switch(logstream_ret) {
		case 0:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, "OK", TRUE);
			break;
		case 2:
			agh_cmd_answer_addtext(cmd, "ALREADY_ACTIVE", TRUE);
			break;
		default:
			agh_cmd_answer_addtext(cmd, g_strdup_printf("LOGSTREAM_INTERNAL_ERROR=%" G_GINT16_FORMAT"", logstream_ret), FALSE);
			break;
	}

wayout:
	return retval;
}

/*
 * Deactivates log streaming.
 *
 * Returns: an integer with value 0 on success, or 101 when ubus context was NULL.
*/
static gint agh_ubus_cmd_logstream_disable_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;
	gint logstream_ret;

	retval = 0;

	/* how did we get here? */
	if (!mstate->uctx) {
		agh_log_ubus_handler_crit("something apparently impossible happened while deactivating logstream: mstate->uctx was NULL!");
		retval = 101;
		goto wayout;
	}

	logstream_ret = agh_ubus_logstream_deinit(mstate->uctx);

	switch(logstream_ret) {
		case 0:
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, "OK", TRUE);
			break;
		case 5:
			agh_cmd_answer_addtext(cmd, "NOT_ACTIVE", TRUE);
			break;
		default:
			agh_cmd_answer_addtext(cmd, g_strdup_printf("LOGSTREAM_INTERNAL_ERROR=%" G_GINT16_FORMAT"", logstream_ret), FALSE);
			break;
	}

wayout:
	return retval;
}

static const struct agh_cmd_operation agh_ubus_handler_logstream_subcommands[] = {
	{
		.op_name = AGH_CMD_UBUS_LOGSTREAM_ACTIVATE,
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_ubus_cmd_logstream_enable_cb
	},
	{
		.op_name = AGH_CMD_UBUS_LOGSTREAM_DEACTIVATE,
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_ubus_cmd_logstream_disable_cb
	},

	{ }
};

static gint agh_ubus_cmd_logstream_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	config_setting_t *arg;
	const gchar *argstr;
	gint retval;

	retval = 0;

	/* This code only serves the purpose of answering something to the user. */
	arg = agh_cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);
	if (!arg) {
		agh_cmd_answer_addtext(cmd, "?", TRUE);
		retval = 101;
		goto wayout;
	}

	agh_cmd_op_match(mstate, agh_ubus_handler_logstream_subcommands, cmd, 2);

wayout:
	return retval;
}

/* subcommands */
static const struct agh_cmd_operation agh_ubus_handler_subcommands[] = {
	{
		.op_name = AGH_CMD_UBUS_LIST,
		.min_args = 0,
		.max_args = 1,
		.cmd_cb = agh_ubus_cmd_list_cb
	},
	{
		.op_name = AGH_CMD_UBUS_CALL,
		.min_args = 1,
		.max_args = 3,
		.cmd_cb = agh_ubus_cmd_call_cb
	},
	{
		.op_name = AGH_CMD_UBUS_LISTEN,
		.min_args = 0,
		.max_args = 2,
		.cmd_cb = agh_ubus_cmd_listen_cb
	},
	{
		.op_name = AGH_CMD_UBUS_LOGSTREAM,
		.min_args = 1,
		.max_args = 1,
		.cmd_cb = agh_ubus_cmd_logstream_cb
	},

	{ }
};

static gint agh_ubus_cmd_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
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

struct agh_message *agh_core_ubus_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;
	struct agh_cmd *cmd;
	struct agh_message *answer;

	cmd = NULL;
	answer = NULL;

	if ((m->msg_type != MSG_SENDCMD) || (!mstate->uctx))
		goto wayout;

	cmd = m->csp;
	agh_cmd_op_match(mstate, agh_ubus_handler_ops, cmd, 0);

wayout:
	if (cmd)
		answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);
	return answer;
}
