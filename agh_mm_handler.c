#include "agh_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_modem.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handler.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_MM_HANDLER domain. */
#define AGH_LOG_DOMAIN_MM_HANDLER "MM_HANDLER"

/* Logging macros. */
#define agh_log_mm_handler_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MM_HANDLER, message, ##__VA_ARGS__)
#define agh_log_mm_handler_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MM_HANDLER, message, ##__VA_ARGS__)

static gint agh_mm_handler_list_modems(struct agh_state *mstate, struct agh_cmd *cmd) {
	guint modem_list_length;
	GList *modems;
	GList *l;
	gint retval;

	retval = 100;
	modems = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager || !cmd) {
		agh_log_mm_handler_dbg("missing context");
		retval = 170;
		goto out;
	}

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mstate->mmstate->manager));

	if (!modems) {
		agh_log_mm_handler_dbg("no modems");
		retval = 175;
		agh_cmd_answer_addtext(cmd, "NO_MODEMS", TRUE);
		goto out;
	}

	/* a NULL list is valid, hence this is pointless as of now */
	modem_list_length = g_list_length(modems);
	if (!modem_list_length) {
		agh_log_mm_handler_crit("non-NULL GList with 0 length?");
		retval = 176;
		goto out;
	}

	agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);

	for (l = modems; l; l = g_list_next (l)) {
		agh_cmd_answer_addtext(cmd, agh_mm_modem_to_index(mm_object_get_path(MM_OBJECT(l->data))), FALSE);
	}

out:
	g_list_free_full(modems, g_object_unref);
	return retval;
}

static void agh_mm_handler_sim_change_pin_cb_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_change_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "CHANGE_PIN_OK");
			break;
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "CHANGE_PIN_FAIL");
	}

	return;
}

static gint agh_mm_handler_sim_change_pin_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	config_setting_t *old_pin_code;
	config_setting_t *new_pin_code;
	const gchar *old_pin_code_str;
	const gchar *new_pin_code_str;

	old_pin_code_str = NULL;
	new_pin_code_str = NULL;

	if (mmstate->sim) {
		old_pin_code = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);
		new_pin_code = agh_cmd_get_arg(cmd, 5, CONFIG_TYPE_STRING);

		if (old_pin_code)
			old_pin_code_str = config_setting_get_string(old_pin_code);

		if (new_pin_code)
			new_pin_code_str = config_setting_get_string(new_pin_code);

		if (old_pin_code_str && new_pin_code_str) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			mm_sim_change_pin(mmstate->sim, old_pin_code_str, new_pin_code_str, NULL, (GAsyncReadyCallback)agh_mm_handler_sim_change_pin_cb_finish, mstate);
		}
	}

	return 100;
}

static void agh_mm_handler_sim_enable_pin_cb_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_enable_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "ENABLE_PIN_OK");
			break;
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "ENABLE_PIN_FAIL");
	}
	return;
}

static gint agh_mm_handler_sim_enable_pin_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	config_setting_t *arg;

	if (mmstate->sim) {
		if ( (arg = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING)) ) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			mm_sim_enable_pin(mmstate->sim, config_setting_get_string(arg), NULL, (GAsyncReadyCallback)agh_mm_handler_sim_enable_pin_cb_finish, mstate);
		}
	}

	return 100;
}

static void agh_mm_handler_sim_disable_pin_cb_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_disable_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "DISABLEPIN_OK");
			break;
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "DISABLE_PIN_FAIL");
	}

	return;
}

static gint agh_mm_handler_sim_disable_pin_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	config_setting_t *arg;

	if (mmstate->sim) {
		if ( (arg = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING)) ) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			mm_sim_disable_pin(mmstate->sim, config_setting_get_string(arg), NULL, (GAsyncReadyCallback)agh_mm_handler_sim_disable_pin_cb_finish, mstate);
		}
	}

	return 100;
}

static void agh_mm_handler_sim_send_puk_cb_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_send_puk_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "PUK_OK");
			break;
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "PUK_FAIL");
	}

	return;
}

static gint agh_mm_handler_sim_send_puk_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	config_setting_t *puk_code;
	config_setting_t *pin_code;
	const gchar *puk_code_str;
	const gchar *pin_code_str;

	puk_code_str = NULL;
	pin_code_str = NULL;

	if (mmstate->sim) {
		puk_code = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);
		pin_code = agh_cmd_get_arg(cmd, 5, CONFIG_TYPE_STRING);

		if (puk_code)
			puk_code_str = config_setting_get_string(puk_code);

		if (pin_code)
			pin_code_str = config_setting_get_string(pin_code);

		if (puk_code_str && pin_code_str) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			mm_sim_send_puk(mmstate->sim, puk_code_str, pin_code_str, NULL, (GAsyncReadyCallback)agh_mm_handler_sim_send_puk_cb_finish, mstate);
		}
	}

	return 100;
}

static void agh_mm_handler_sim_send_pin_cb_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_send_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "PIN_OK");
			break;
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_sim_get_path(sim)), "PIN_FAIL");
	}
	return;
}

static gint agh_mm_handler_sim_send_pin_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	config_setting_t *arg;

	if (mmstate->sim) {
		if ( (arg = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING)) ) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			mm_sim_send_pin(mmstate->sim, config_setting_get_string(arg), NULL, (GAsyncReadyCallback)agh_mm_handler_sim_send_pin_cb_finish, mstate);
		}
	}

	return 100;
}

static gint agh_mm_handler_sim_operator_name_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->sim) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_sim_get_operator_name(mmstate->sim), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_sim_operator_id_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->sim) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_sim_get_operator_identifier(mmstate->sim), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_sim_imsi_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->sim) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_sim_get_imsi(mmstate->sim), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_sim_id_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->sim) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_sim_get_identifier(mmstate->sim), TRUE);
	}

	return 100;
}

static const struct agh_cmd_operation agh_modem_sim_ops[] = {
	{
		.op_name = "id",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_sim_id_cb
	},
	{
		.op_name = "imsi",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_sim_imsi_cb
	},
	{
		.op_name = "operator_id",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_sim_operator_id_cb
	},
	{
		.op_name = "operator_name",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_sim_operator_name_cb
	},
	{
		.op_name = "send_pin",
		.min_args = 1,
		.max_args = 1,
		.cmd_cb = agh_mm_handler_sim_send_pin_cb
	},
	{
		.op_name = "send_puk",
		.min_args = 2,
		.max_args = 2,
		.cmd_cb = agh_mm_handler_sim_send_puk_cb
	},
	{
		.op_name = "disable_pin",
		.min_args = 1,
		.max_args = 1,
		.cmd_cb = agh_mm_handler_sim_disable_pin_cb
	},
	{
		.op_name = "enable_pin",
		.min_args = 1,
		.max_args = 1,
		.cmd_cb = agh_mm_handler_sim_enable_pin_cb
	},
	{
		.op_name = "change_pin",
		.min_args = 2,
		.max_args = 2,
		.cmd_cb = agh_mm_handler_sim_change_pin_cb
	},

	{ }
};

static gint agh_mm_handler_cmd_unstore(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->current_cmd) {
		agh_log_mm_handler_crit("missing context or command");
		retval = 153;
		goto out;
	}

	retval = agh_cmd_free(mstate->mmstate->current_cmd);
	mstate->mmstate->current_cmd = NULL;

out:
	return retval;
}

static gint agh_mm_handler_cmd_store(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate || mstate->mmstate->current_cmd || !cmd) {
		agh_log_mm_handler_crit("missing context, an agh_cmd struct is already present when it is not supposed to, or the passed one is NULL");
		retval = 151;
		goto out;
	}

	mstate->mmstate->current_cmd = agh_cmd_copy(cmd);
	if (!mstate->mmstate->current_cmd) {
		agh_log_mm_handler_crit("command copy failed");
		retval = 152;
		goto out;
	}

out:
	return retval;
}

static void agh_mm_handler_messaging_list_delete_finish(MMModemMessaging *messaging, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_modem_messaging_delete_finish(messaging, res, &mstate->mmstate->current_gerror)) {
		case FALSE:
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
	}

	return;
}

static void agh_mm_handler_messaging_sms_send_cb_with_msms_send_result(MMSms *sms, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sms_send_finish(sms, res, &mstate->mmstate->current_gerror)) {
		case FALSE:
			agh_log_mm_handler_crit("failure sending message");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
			break;
		case TRUE:
			agh_mm_report_event(mstate->comm, "SMS_SENT", agh_mm_modem_to_index(mm_sms_get_path(sms)), ":)");
	}

	return;
}

static void agh_mm_handler_messaging_sms_send_cb_with_msms(MMModemMessaging *messaging, GAsyncResult *res, struct agh_state *mstate) {
	MMSms *sms;

	sms = mm_modem_messaging_create_finish(messaging, res, &mstate->mmstate->current_gerror);
	if (!sms) {
		agh_log_mm_handler_crit("failure creating SMS");
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, mstate->comm);
		return;
	}

	mm_sms_send(sms, NULL, (GAsyncReadyCallback)agh_mm_handler_messaging_sms_send_cb_with_msms_send_result, mstate);
	mm_modem_messaging_delete(messaging, mm_sms_get_path(sms), NULL, (GAsyncReadyCallback)agh_mm_handler_messaging_list_delete_finish, mstate);
	g_object_unref(sms);
}

static gint agh_mm_handler_messaging_sms_send_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMSmsProperties *smsprops;
	const gchar *number;
	const gchar *text;
	config_setting_t *arg_number;
	config_setting_t *arg_text;
	gint retval;

	smsprops = NULL;
	retval = 100;

	if (mmstate->messaging) {
		smsprops = mm_sms_properties_new();
		if (!smsprops) {
			agh_log_mm_handler_crit("failure while getting new SMS properties object");
			retval = 101;
			goto out;
		}

		arg_number = agh_cmd_get_arg(cmd, 4, CONFIG_TYPE_STRING);
		arg_text = agh_cmd_get_arg(cmd, 5, CONFIG_TYPE_STRING);

		if (!arg_number || !arg_text) {
			agh_cmd_answer_addtext(cmd, "MISSING_ARGS", TRUE);
			retval = 102;
			goto out;
		}

		number = config_setting_get_string(arg_number);
		text = config_setting_get_string(arg_text);

		if (!strlen(number) || !strlen(text)) {
			agh_log_mm_handler_crit("zero length text or number?");
			retval = 107;
			goto out;
		}

		mm_sms_properties_set_number(smsprops, number);
		mm_sms_properties_set_text(smsprops, text);

		mm_modem_messaging_create(mmstate->messaging, smsprops, NULL, (GAsyncReadyCallback)agh_mm_handler_messaging_sms_send_cb_with_msms, mstate);
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, "create_req", TRUE);

	}

out:
	if (smsprops)
		g_object_unref(smsprops);

	return retval;
}

static gint agh_mm_handler_messaging_list_delete_all_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	GList *l;

	if (mmstate->messaging && mmstate->smslist) {
		for (l = mmstate->smslist; l; l = g_list_next (l)) {
			mm_modem_messaging_delete(mmstate->messaging, mm_sms_get_path(MM_SMS(l->data)), NULL, (GAsyncReadyCallback)agh_mm_handler_messaging_list_delete_finish, mstate);
		}
	}
	agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(cmd, "delete_all", TRUE);
	return 100;
}

static const struct agh_cmd_operation agh_modem_messaging_list_ops[] = {
	{
		.op_name = "da",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_messaging_list_delete_all_cb
	},
	{
		.op_name = "send",
		.min_args = 2,
		.max_args = 2,
		.cmd_cb = agh_mm_handler_messaging_sms_send_cb
	},

	{ }
};

static void agh_mm_handler_modem_sms_message_gate_exit_cb(MMModemMessaging *messaging, GAsyncResult *res, struct agh_state *mstate) {
	GList *smslist;
	GList *l;
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct agh_message *gate_msg;
	config_setting_t *arg;

	smslist = NULL;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_handler_crit("no AGH ( / MM state)");
		return;
	}

	smslist = mm_modem_messaging_list_finish(messaging, res, &mmstate->current_gerror);
	if (!smslist) {
		agh_log_mm_handler_crit("unable to get SMS list for %s",mm_modem_messaging_get_path(messaging));
		agh_modem_report_gerror_message(&mmstate->current_gerror, mstate->comm);
	}
	mmstate->smslist = smslist;

	if ( (arg = agh_cmd_get_arg(mmstate->current_cmd, 3, CONFIG_TYPE_STRING)) ) {
		agh_log_mm_handler_dbg("SMS global commands");
		agh_cmd_op_match(mstate, agh_modem_messaging_list_ops, mmstate->current_cmd, 3);
	}
	else
		if ( (arg = agh_cmd_get_arg(mmstate->current_cmd, 3, CONFIG_TYPE_INT)) ) {
			agh_log_mm_handler_dbg("should search for message");
			/* if smslist is not NULL, then ... */
		}
		else {

			if (mmstate->smslist) {
				agh_cmd_answer_set_status(mmstate->current_cmd, AGH_CMD_ANSWER_STATUS_OK);
				agh_cmd_answer_addtext(mmstate->current_cmd, "LIST_OK", TRUE);

				for (l = smslist; l; l = g_list_next (l)) {
					agh_mm_report_sms(mstate->comm, MM_SMS(l->data));
				}
			}

		}

	mmstate->smslist = NULL;
	if (smslist) {
		agh_log_mm_handler_dbg("unreferencing messages list");
		g_list_free_full(smslist, g_object_unref);
	}

	if (mmstate->messaging) {
		agh_log_mm_handler_dbg("unreferencing messaging object");
		g_object_unref(mmstate->messaging);
		mmstate->messaging = NULL;
	}
	else
		agh_log_mm_handler_crit("messaging object not found?");

	if (!mstate->comm->teardown_in_progress) {
		gate_msg = agh_cmd_answer_msg(mmstate->current_cmd, mstate->comm, NULL);
		if (agh_msg_send(gate_msg, mstate->comm, NULL)) {
			agh_msg_dealloc(gate_msg);
		}
	}

	agh_mm_handler_cmd_unstore(mstate);

	return;
}

static gint agh_mm_handler_modem_sms_message_gate_enter_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		if (!agh_mm_handler_cmd_store(mstate, cmd)) {
			mmstate->messaging = mm_object_get_modem_messaging(mmstate->mmobject);
			if (mmstate->messaging) {
				mm_modem_messaging_list(mmstate->messaging, NULL, (GAsyncReadyCallback)agh_mm_handler_modem_sms_message_gate_exit_cb, mstate);
				agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
				agh_cmd_answer_addtext(cmd, "async_SMS_message_gate_traversal", TRUE);
			}
		}
	}

	return 100;
}

static void agh_mm_handler_modem_sim_gate_exit_cb(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	MMSim *sim;
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct agh_message *gate_msg;

	sim = NULL;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_handler_crit("no AGH ( / MM state)");
		return;
	}

	sim = mm_modem_get_sim_finish(modem, res, &mmstate->current_gerror);
	if (!sim) {
		agh_log_mm_handler_crit("unable to get SIM for modem %s",mm_modem_get_path(modem));
		agh_modem_report_gerror_message(&mmstate->current_gerror, mstate->comm);
		goto out;
	}

	mmstate->sim = sim;
	agh_cmd_op_match(mstate, agh_modem_sim_ops, mmstate->current_cmd, 3);

out:
	if (sim) {
		g_object_unref(sim);
		mmstate->sim = NULL;
	}

	if (!mstate->comm->teardown_in_progress) {
		gate_msg = agh_cmd_answer_msg(mmstate->current_cmd, mstate->comm, NULL);
		if (agh_msg_send(gate_msg, mstate->comm, NULL)) {
			agh_msg_dealloc(gate_msg);
		}
	}

	agh_mm_handler_cmd_unstore(mstate);

	return;
}

static gint agh_mm_handler_modem_sim_gate_enter_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		if (!agh_mm_handler_cmd_store(mstate, cmd)) {
			mm_modem_get_sim(mmstate->modem, NULL, (GAsyncReadyCallback)agh_mm_handler_modem_sim_gate_exit_cb, mstate);
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, "async_SIM_gate_traversal", TRUE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_access_technology_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_addtext(cmd, mm_modem_access_technology_build_string_from_mask(mm_modem_get_access_technologies(mmstate->modem)), FALSE);
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
	}

	return 100;
}

static gint agh_mm_handler_modem_signal_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gboolean recent;
	guint signal_quality;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		signal_quality = mm_modem_get_signal_quality(mmstate->modem, &recent);
		agh_cmd_answer_addtext(cmd, g_strdup_printf("%" G_GUINT16_FORMAT"",signal_quality), FALSE);
		if (recent)
			agh_cmd_answer_addtext(cmd, "is_recent", TRUE);
		else
			agh_cmd_answer_addtext(cmd, "is_not_recent", TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_ip_families_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMBearerIpFamily family;

	if (mmstate->modem) {
		agh_cmd_answer_addtext(cmd, mm_bearer_ip_family_build_string_from_mask(mm_modem_get_supported_ip_families(mmstate->modem)), FALSE);
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
	}

	return 100;
}

static gint agh_mm_handler_modem_bands_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMModemBand *bands;
	guint n_bands;

	if (mmstate->modem) {
		if (mm_modem_get_supported_bands(mmstate->modem, &bands, &n_bands)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, agh_mm_common_build_bands_string(bands, n_bands), FALSE);
			g_free(bands);
		}
		if (mm_modem_get_current_bands(mmstate->modem, &bands, &n_bands)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, agh_mm_common_build_bands_string(bands, n_bands), FALSE);
			g_free(bands);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_modes_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMModemModeCombination *modes;
	guint n_modes;
	MMModemMode allowed_modes;
	MMModemMode preferred_mode;

	if (mmstate->modem) {
		if (mm_modem_get_supported_modes(mmstate->modem, &modes, &n_modes)) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, agh_mm_common_build_mode_combinations_string(modes, n_modes), FALSE);
			g_free(modes);
		}
		if (mm_modem_get_current_modes(mmstate->modem, &allowed_modes, &preferred_mode)) {
			agh_cmd_answer_addtext(cmd, mm_modem_mode_build_string_from_mask(allowed_modes), FALSE);
			agh_cmd_answer_addtext(cmd, mm_modem_mode_build_string_from_mask(preferred_mode), FALSE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_own_numbers_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gchar *mm_own_numbers;

	if (mmstate->modem) {
		mm_own_numbers = g_strjoinv (", ", (gchar **)mm_modem_get_own_numbers(mmstate->modem));
		if (mm_own_numbers) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, mm_own_numbers, FALSE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_bearer_paths_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gchar *mm_bpaths;

	if (mmstate->modem) {
		mm_bpaths = g_strjoinv (", ", (gchar **)mm_modem_get_bearer_paths(mmstate->modem));
		if (mm_bpaths) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, mm_bpaths, FALSE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_max_bearers_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, g_strdup_printf("maxdefined=%" G_GUINT16_FORMAT", maxactive=%" G_GUINT16_FORMAT"",mm_modem_get_max_bearers(mmstate->modem), mm_modem_get_max_active_bearers(mmstate->modem)), FALSE);
	}

	return 100;
}

static gint agh_mm_handler_modem_unlock_retries_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMUnlockRetries *retries;
	gchar *retries_str;

	if (mmstate->modem) {
		retries = mm_modem_get_unlock_retries(mmstate->modem);
		if (retries) {
			retries_str = agh_mm_unlock_retries_build_string(retries);
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, retries_str, FALSE);
			g_object_unref(retries);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_unlock_required_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_lock_get_string(mm_modem_get_unlock_required(mmstate->modem)), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_device_identifier_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_device_identifier(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_equipment_identifier_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_equipment_identifier(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_device_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_device(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_ports_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMModemPortInfo *ports;
	gint retval;
	gchar *ports_str;
	guint n_ports;

	retval = 100;

	if (mmstate->modem) {
		if (!mm_modem_get_ports(mmstate->modem, &ports, &n_ports)) {
			retval++;
			return retval;
		}

		ports_str = agh_mm_common_build_ports_string(ports, n_ports);
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, ports_str, FALSE);
		mm_modem_port_info_array_free(ports, n_ports);
	}

	return 100;
}

static gint agh_mm_handler_modem_primary_port_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_primary_port(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_plugin_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_plugin(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_drivers_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gchar *mm_drivers;

	if (mmstate->modem) {
		mm_drivers = g_strjoinv (", ", (gchar **)mm_modem_get_drivers(mmstate->modem));
		if (mm_drivers) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, mm_drivers, FALSE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_revision_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_revision(mmstate->modem), TRUE);
		agh_cmd_answer_addtext(cmd, mm_modem_get_hardware_revision(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_model_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_model(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_manifacturer_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_get_manufacturer(mmstate->modem), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_current_caps_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gchar *caps;

	if (mmstate->modem) {
		caps = mm_modem_capability_build_string_from_mask(mm_modem_get_current_capabilities(mmstate->modem));
		if (caps) {
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, caps, FALSE);
		}
	}

	return 100;
}

static gint agh_mm_handler_modem_supported_caps_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;
	struct agh_mm_state *mmstate = mstate->mmstate;
	gchar *caps_str;
	MMModemCapability *caps;
	guint n_caps;

	retval = 100;

	if (mmstate->modem) {
		if (!mm_modem_get_supported_capabilities(mmstate->modem, &caps, &n_caps)) {
			retval++;
		}
		else {
			caps_str = agh_mm_common_build_capabilities_string(caps, n_caps);
			agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
			agh_cmd_answer_addtext(cmd, caps_str, FALSE);
			g_free(caps);
		}
	}

	return retval;
}

static gint agh_mm_handler_modem_getstate_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_state_get_string(mm_modem_get_state(mmstate->modem)), TRUE);
		agh_cmd_answer_addtext(cmd, mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(mmstate->modem)), TRUE);
	}

	return 100;
}

static gint agh_mm_handler_modem_get_power_state_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	if (mmstate->modem) {
		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_OK);
		agh_cmd_answer_addtext(cmd, mm_modem_power_state_get_string(mm_modem_get_power_state(mmstate->modem)), TRUE);
	}

	return 100;
}

static const struct agh_cmd_operation agh_modem_ops[] = {
	{
		.op_name = "state",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_getstate_cb
	},
	{
		.op_name = "powerstate",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_get_power_state_cb
	},
	{
		.op_name = "supported_caps",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_supported_caps_cb
	},
	{
		.op_name = "current_caps",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_current_caps_cb
	},
	{
		.op_name = "manifacturer",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_manifacturer_cb
	},
	{
		.op_name = "model",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_model_cb
	},
	{
		.op_name = "revision",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_revision_cb
	},
	{
		.op_name = "drivers",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_drivers_cb
	},
	{
		.op_name = "plugin",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_plugin_cb
	},
	{
		.op_name = "primary_port",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_primary_port_cb
	},
	{
		.op_name = "ports",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_ports_cb
	},
	{
		.op_name = "device",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_device_cb
	},
	{
		.op_name = "equipment_identifier",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_equipment_identifier_cb
	},
	{
		.op_name = "device_identifier",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_device_identifier_cb
	},
	{
		.op_name = "unlock_required",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_unlock_required_cb
	},
	{
		.op_name = "unlock_retries",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_unlock_retries_cb
	},
	{
		.op_name = "max_bearers",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_max_bearers_cb
	},
	{
		.op_name = "bearers",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_bearer_paths_cb
	},
	{
		.op_name = "numbers",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_own_numbers_cb
	},
	{
		.op_name = "modes",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_modes_cb
	},
	{
		.op_name = "bands",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_bands_cb
	},
	{
		.op_name = "ip_families",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_ip_families_cb
	},
	{
		.op_name = "signal",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_signal_cb
	},
	{
		.op_name = "access",
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_mm_handler_modem_access_technology_cb
	},
	{
		.op_name = "sim",
		.min_args = 1,
		.max_args = 3,
		.cmd_cb = agh_mm_handler_modem_sim_gate_enter_cb
	},
	{
		.op_name = "sms",
		.min_args = 0,
		.max_args = 3,
		.cmd_cb = agh_mm_handler_modem_sms_message_gate_enter_cb
	},

	{ }
};

static MMObject *agh_mm_handler_index_to_modem(struct agh_state *mstate, gint modem_index) {
	gchar *modem_path;
	guint modem_list_length;
	GList *modems;
	MMObject *current_modem;
	GList *l;
	MMObject *modem_found;

	modem_found = NULL;

	if (modem_index < 0) {
		agh_log_mm_handler_dbg("modem index should be positive");
		return modem_found;
	}

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mstate->mmstate->manager));

	if (!modems) {
		agh_log_mm_handler_dbg("no modems");
		return modem_found;
	}

	/* a NULL list is valid, hence this is pointless as of now */
	modem_list_length = g_list_length(modems);

	if (!modem_list_length) {
		g_list_free(modems);
		return modem_found;
	}

	modem_path = g_strdup_printf(MM_DBUS_MODEM_PREFIX"/%" G_GINT16_FORMAT"", modem_index);

	for (l = modems; l; l = g_list_next (l)) {
		current_modem = MM_OBJECT(l->data);
		if (!g_strcmp0(mm_object_get_path(current_modem), modem_path)) {
			modem_found = current_modem;
			modems = g_list_remove(modems, l->data);
			break;
		}
	}

	g_free(modem_path);
	g_list_free_full(modems, g_object_unref);
	return modem_found;
}

static gint agh_mm_handler_release_objects(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_handler_crit("can not release objects when missing AGH or AGH MMcontext");
		retval = 80;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->mmobject)
		g_clear_pointer(&mmstate->mmobject, g_object_unref);

	if (mmstate->modem)
		g_clear_pointer(&mmstate->modem, g_object_unref);

	if (mmstate->modem3gpp)
		g_clear_pointer(&mmstate->modem3gpp, g_object_unref);

	if (mmstate->modem3gppussd)
		g_clear_pointer(&mmstate->modem3gppussd, g_object_unref);

	if (mmstate->modemlocation)
		g_clear_pointer(&mmstate->modemlocation, g_object_unref);

	if (mmstate->time)
		g_clear_pointer(&mmstate->time, g_object_unref);

	if (mmstate->firmware)
		g_clear_pointer(&mmstate->firmware, g_object_unref);

	if (mmstate->oma)
		g_clear_pointer(&mmstate->oma, g_object_unref);

	if (mmstate->signal)
		g_clear_pointer(&mmstate->signal, g_object_unref);

	if (mmstate->voice)
		g_clear_pointer(&mmstate->voice, g_object_unref);

out:
	return retval;
}

static gint agh_mm_handler_get_objects(struct agh_state *mstate, gint index) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_handler_crit("no AGH / (AGH MM) context to operate on");
		retval = 71;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->mmobject) {
		agh_log_mm_handler_crit("%s was called with a MMObject already present",__FUNCTION__);
		retval = 72;
		goto out;
	}

	mmstate->mmobject = agh_mm_handler_index_to_modem(mstate, index);
	if (!mmstate->mmobject) {
		agh_log_mm_handler_dbg("can not find an object representing this modem");
		retval = 73;
		goto out;
	}

	mmstate->modem = mm_object_get_modem(mmstate->mmobject);

	mmstate->modem3gpp = mm_object_get_modem_3gpp(mmstate->mmobject);

	mmstate->modem3gppussd = mm_object_get_modem_3gpp_ussd(mmstate->mmobject);

	mmstate->modemlocation = mm_object_get_modem_location(mmstate->mmobject);

	mmstate->time = mm_object_get_modem_time(mmstate->mmobject);

	mmstate->firmware = mm_object_get_modem_firmware(mmstate->mmobject);

	mmstate->oma = mm_object_get_modem_oma(mmstate->mmobject);

	mmstate->signal = mm_object_get_modem_signal(mmstate->mmobject);

	mmstate->voice = mm_object_get_modem_voice(mmstate->mmobject);

out:
	if (retval)
		agh_mm_handler_release_objects(mstate);

	return retval;
}

static gint agh_mm_handler_cmd_cb(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;
	config_setting_t *arg;
	gint modem_index;
	MMObject *m;

	retval = 0;

	if ( (arg = agh_cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING)) ) {
		agh_log_mm_handler_dbg("global commands not yet implemented");
	}
	else
		if ( (arg = agh_cmd_get_arg(cmd, 1, CONFIG_TYPE_INT)) ) {
			agh_log_mm_handler_dbg("should search for modem");
			if ( (retval = agh_mm_handler_get_objects(mstate, config_setting_get_int(arg)) )) {
				agh_log_mm_handler_crit("failed to get objects");
				return 100+retval;
			}

			agh_cmd_op_match(mstate, agh_modem_ops, cmd, 2);

			agh_mm_handler_release_objects(mstate);
		}
		else {
			agh_mm_handler_list_modems(mstate, cmd);
		}

	return 100+retval;
}

static const struct agh_cmd_operation agh_mm_handler_ops[] = {
	{
		.op_name = "modem",
		.min_args = 0,
		.max_args = 5,
		.cmd_cb = agh_mm_handler_cmd_cb
	},

	{ }
};

struct agh_message *agh_mm_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;
	struct agh_message *answer;
	struct agh_cmd *cmd;
	config_setting_t *arg;

	answer = NULL;

	if ((!mstate || !mstate->mmstate || !mstate->mmstate->manager) || (m->msg_type != MSG_SENDCMD))
		goto out;

	cmd = m->csp;
	agh_cmd_op_match(mstate, agh_mm_handler_ops, cmd, 0);

	answer = agh_cmd_answer_msg(cmd, mstate->comm, NULL);

out:
	return answer;
}
