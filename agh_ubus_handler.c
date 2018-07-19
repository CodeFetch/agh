#include <libubox/blobmsg_json.h>
#include "handlers.h"
#include "messages.h"
#include "commands.h"
#include "agh_ubus.h"
#include "agh_ubus_handler.h"
#include "agh_ubus_helpers.h"

gpointer agh_core_ubus_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_state *mstate = h->handler_data;
	struct agh_message *m = hmessage;

	struct command *cmd;
	struct agh_ubus_ctx *uctx;
	struct agh_message *answer;
	void (*agh_ubus_cmd_handler_cb)(struct agh_ubus_ctx *uctx, struct command *cmd);
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
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_NO_CONNECTION);
		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);
	if (!arg) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_MISSING_SUBCOMMAND);
		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	argstr = config_setting_get_string(arg);
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LIST))
		agh_ubus_cmd_handler_cb = agh_ubus_list;
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_CALL))
		agh_ubus_cmd_handler_cb = agh_ubus_call;
	if (!g_strcmp0(argstr, AGH_CMD_UBUS_LISTEN))
		agh_ubus_cmd_handler_cb = agh_ubus_listen;

	if (!agh_ubus_cmd_handler_cb) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_INVALID_SUBCOMMAND);
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
void agh_ubus_list(struct agh_ubus_ctx *uctx, struct command *cmd) {
	const gchar *path;
	config_setting_t *arg;

	path = NULL;
	arg = NULL;

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);

	ubus_lookup(uctx->ctx, path, agh_ubus_list_receive_results, cmd);

	cmd_answer_if_empty(cmd, CMD_ANSWER_STATUS_FAIL, AGH_UBUS_NO_DATA, FALSE);

	return;
}

/* The name and much of the code in this function has been inspired by ubus cli.c, function "receive_list_result". */
void agh_ubus_list_receive_results(struct ubus_context *ctx, struct ubus_object_data *obj, gpointer data) {
	struct command *cmd = data;

	guint rem;
	struct blob_attr *cur;
	gchar *tmp;

	cur = NULL;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, g_strdup_printf("\n\"OBJECT=%s, ID=@%08x\"\n", obj->path, obj->id));

	if (!obj->signature) {
		g_print("%s: no signature fo %s\n",__FUNCTION__,obj->path);
		return;
	}

	blob_for_each_attr(cur, obj->signature, rem) {
		tmp = blobmsg_format_json_with_cb(cur, false, agh_ubus_helper_format_type, NULL, -1);

		if (tmp) {
			cmd_answer_peektext(cmd, g_strdup_printf("%s\n", tmp));
			g_free(tmp);
			tmp = NULL;
		}

	}

	cmd_answer_set_data(cmd, TRUE);

	return;
}

void agh_ubus_call(struct agh_ubus_ctx *uctx, struct command *cmd) {
	const gchar *path;
	config_setting_t *arg;
	const gchar *method;
	const gchar *message;
	struct blob_buf *bbuf;
	guint32 id;

	path = NULL;
	method = NULL;
	message = NULL;
	arg = NULL;
	bbuf = NULL;
	id = 0;

	arg = cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING);

	if (arg)
		path = config_setting_get_string(arg);
	else {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_MISSING_PATH);
		return;
	}

	arg = cmd_get_arg(cmd, 3, CONFIG_TYPE_STRING);

	if (arg)
		method = config_setting_get_string(arg);
	else {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_MISSING_METHOD);
		return;
	}

	arg = cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);

	bbuf = g_malloc0(sizeof(struct blob_buf));

	if (arg)
		message = config_setting_get_string(arg);

	if (blob_buf_init(bbuf, 0)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_BBUF_INIT_ERROR);
		g_free(bbuf);
		return;
	}

	if (message) {
		if (!blobmsg_add_json_from_string(bbuf, message)) {
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_INVALID_JSON_MESSAGE);
			blob_buf_free(bbuf);
			g_free(bbuf);
			return;
		}
	}

	if (ubus_lookup_id(uctx->ctx, path, &id)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_UBUS_METHOD_NOT_FOUND);
		blob_buf_free(bbuf);
		g_free(bbuf);
		return;
	}

	ubus_invoke(uctx->ctx, id, method, bbuf->head, agh_receive_call_result_data, NULL, 2 * 1000);

	if (agh_ubus_call_data_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_peektext(cmd, g_strdup_printf("\n%s", agh_ubus_call_data_str));

		/* agh_receive_call_result_data may not be called at all, so this is needed to avoid returning stale data */
		g_free(agh_ubus_call_data_str);
		agh_ubus_call_data_str = NULL;
	}

	blob_buf_free(bbuf);
	g_free(bbuf);
	cmd_answer_set_data(cmd, TRUE);

	cmd_answer_if_empty(cmd, CMD_ANSWER_STATUS_FAIL, AGH_UBUS_NO_DATA, FALSE);

	return;
}

void agh_ubus_listen(struct agh_ubus_ctx *uctx, struct command *cmd) {
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
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_NOT_ENABLED);
			return;
		}

		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
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
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_FAILED);
			return;
		}
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_OK);
		return;
	}

	/* Or disable events reporting. */
	if (!g_strcmp0(arg_str, AGH_CMD_UBUS_LISTEN_STOP)) {
		if (agh_ubus_event_disable(uctx)) {
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_FAILED);
			return;
		}
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_OK);
		return;
	}

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
	cmd_answer_addtext(cmd, AGH_UBUS_HANDLER_EVENTS_UNKNOWN_SUBCOMMAND);
	return;
}

void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg) {
	struct command *agh_event;
	gchar *event_message;

	agh_event = cmd_event_prepare();
	event_message = NULL;

	event_message = blobmsg_format_json(msg, true);

	cmd_answer_set_data(agh_event, TRUE);
	cmd_answer_set_status(agh_event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(agh_event, AGH_UBUS_HANDLER_UBUS_EVENTs_NAME);
	cmd_answer_peektext(agh_event, g_strdup_printf("\n{ \"%s\": %s }\n", type, event_message));
	cmd_emit_event(agh_ubus_aghcomm, agh_event);
	g_free(event_message);

	return;
}
