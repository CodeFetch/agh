#include "handlers.h"
#include "messages.h"
#include "commands.h"
#include "modem.h"
#include "modem_mm_helpers.h"
#include "modem_handlers.h"

gpointer modem_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_thread *ct = h->handler_data;
	struct modem_state *mmstate = ct->thread_data;
	struct command *cmd;
	const gchar *string_arg;
	gint current_modem;
	config_setting_t *arg;
	struct agh_message *answer;
	MMObject *modem;

	cmd = NULL;
	string_arg = NULL;
	current_modem = 0;
	arg = NULL;
	answer = NULL;
	modem = NULL;

	if (m->msg_type != MSG_SENDCMD)
		return cmd;

	cmd = m->csp;

	if (!g_strcmp0(cmd_get_operation(cmd), AGH_CMD_QUIT)) {
		g_main_loop_quit(ct->evl);
		return NULL; /* reached */
	}

	/* If this is not the AGH_CMD_MODEM command, then stop here. */
	if (g_strcmp0(cmd_get_operation(cmd), AGH_CMD_MODEM))
		return NULL;

	cmd_answer_prepare(cmd);

	/* We can not act on commands if talking with ModemManager is not possible. */
	if (!mmstate->manager) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_NO_MM_PROCESS_TEXT);
		answer = cmd_answer_msg(cmd, ct->comm, ct->agh_comm);
		return answer;
	}

	/* If an integer was specified, then this is the modem on which we're supposed to operate. Otherwise it's a subcommand. */
	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_INT);
	if (arg) {
		/* A modem was specified. */
		current_modem = config_setting_get_int(arg);
		modem = agh_mm_index_to_modem(mmstate, current_modem);

		if (modem) {
			g_print("Modem %" G_GINT16_FORMAT" has been specified.\n",current_modem);
			agh_modem_do(modem, cmd);
			g_object_unref(modem);
			modem = NULL;
		} else {
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_MM_INVALID_MODEM);
		}

		answer = cmd_answer_msg(cmd, ct->comm, ct->agh_comm);
		return answer;
	}

	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);

	if (arg) {
		string_arg = config_setting_get_string(arg);
		g_print("General subcommand was specified: %s\n",string_arg);
		return NULL;
	}

	agh_mm_list_modems(mmstate, cmd);

	answer = cmd_answer_msg(cmd, ct->comm, ct->agh_comm);

	/* I acknowledge answer may be NULL. */

	return answer;
}

/* Get and report a list of modems known by ModemManager. */
void agh_mm_list_modems(struct modem_state *mmstate, struct command *cmd) {
	GList *modems;

	modems = NULL;

	modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (mmstate->manager));

	if (!modems) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		return;
	}
	else {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		g_list_foreach(modems, agh_mm_list_modem_single, cmd);
		g_list_free_full(modems, g_object_unref);
	}

	modems = NULL;

	return;
}

void agh_mm_list_modem_single(gpointer data, gpointer user_data) {
	MMObject *modem_object = data;
	struct command *cmd = user_data;
	gchar *modem_index;

	modem_index = agh_mm_modem_to_index(mm_object_get_path(modem_object));

	cmd_answer_addtext(cmd, modem_index);
	g_free(modem_index);

	return;
}

/* Input to this function should come from MM calls, in general. That's why no validation is done. */
gchar *agh_mm_modem_to_index(const gchar *modem_path) {
	GString *modem_index;
	gsize modem_path_size;
	gint i;

	modem_index = NULL;
	modem_path_size = 0;

	if (!modem_path)
		return NULL;

	modem_index = g_string_new(NULL);

	modem_path_size = strlen(modem_path);

	for (i=modem_path_size-1;i>=0;i--) {
		if (modem_path[i] == '/')
			break;

		g_string_append_c(modem_index, modem_path[i]);
	}

	return g_string_free(modem_index, FALSE);
}

MMObject *agh_mm_index_to_modem(struct modem_state *mmstate, gint modem_index) {
	gchar *modem_path;
	guint modem_list_length;
	GList *modems;
	MMObject *current_modem;
	GList *l;
	MMObject *modem_found;

	modem_list_length = 0;
	modems = NULL;
	current_modem = NULL;
	modem_path = NULL;
	l = NULL;
	modem_found = NULL;

	if (modem_index<0)
		return modem_found;

	modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (mmstate->manager));

	if (!modems)
		return modem_found;

	modem_list_length = g_list_length(modems);

	if (!modem_list_length) {
		g_free(modems);
		return modem_found;
	}

	modem_path = g_strdup_printf(MM_DBUS_MODEM_PREFIX"/%" G_GUINT16_FORMAT"", modem_index);

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

void agh_modem_do(MMObject *modem, struct command *cmd) {
	const gchar *current_operation;
	void (*cb)(MMObject *modem, struct command *cmd);

	cb = NULL;
	current_operation = config_setting_get_string(cmd_get_arg(cmd, 2, CONFIG_TYPE_STRING));

	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_IMEI))
		cb = agh_modem_get_imei;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_STATE))
		cb = agh_modem_get_state;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_POWER_STATE))
		cb = agh_modem_get_power_state;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_SUPPORTED_CAPABILITIES))
		cb = agh_modem_get_supported_capabilities;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_CURRENT_CAPABILITIES))
		cb = agh_modem_get_current_capabilities;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_MANIFACTURER))
		cb = agh_modem_get_manifacturer;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_MODEL))
		cb = agh_modem_get_model;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_REVISION))
		cb = agh_modem_get_revision;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_HARDWARE_REVISION))
		cb = agh_modem_get_hw_revision;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_DRIVERS))
		cb = agh_modem_get_drivers;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_PLUGIN))
		cb = agh_modem_get_plugin;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_PRIMARY_PORT))
		cb = agh_modem_get_primary_port;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_PORTS))
		cb = agh_modem_get_ports;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_DEVICE))
		cb = agh_modem_get_device;

	if (!cb) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_INVALID_SUBCOMMAND);
		return;
	}

	cb(modem, cmd);

	return;
}

MMModem3gpp *agh_get_MMModem3gpp_object(MMObject *modem, struct command *cmd) {
	MMModem3gpp *object;

	object = NULL;

	object = mm_object_get_modem_3gpp(modem);

	if (!object) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_NO_MMModem3gpp_OBJECT);
	}

	return object;
}

void agh_modem_get_imei(MMObject *modem, struct command *cmd) {
	MMModem3gpp *object;
	const gchar *imei;

	object = agh_get_MMModem3gpp_object(modem, cmd);
	imei = NULL;

	if (!object)
		return;

	imei = mm_modem_3gpp_get_imei(object);

	if (!imei) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_IMEI_NOT_AVAILABLE);
	}
	else {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, imei);
	}

	g_object_unref(object);
	return;
}

MMModem *agh_get_MMModem_object(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = mm_object_get_modem(modem);

	if (!object) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_NO_MMModem_OBJECT);
	}

	return object;
}

void agh_modem_get_state(MMObject *modem, struct command *cmd) {
	MMModem *object;
	const gchar *state;
	const gchar *state_failed_reason;
	MMModemState enum_state;
	MMModemStateFailedReason enum_state_failed_reason;

	object = NULL;
	state = NULL;
	state_failed_reason = NULL;
	enum_state_failed_reason = MM_MODEM_STATE_FAILED_REASON_NONE;
	enum_state = MM_MODEM_STATE_UNKNOWN;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	enum_state = mm_modem_get_state(object);
	state = VALIDATE_UNKNOWN(mm_modem_state_get_string(enum_state));
	enum_state_failed_reason = mm_modem_get_state_failed_reason(object);
	state_failed_reason = VALIDATE_UNKNOWN(mm_modem_state_failed_reason_get_string(enum_state_failed_reason));

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, state);
	cmd_answer_addtext(cmd, state_failed_reason);

	g_object_unref(object);
	return;
}

void agh_modem_get_power_state(MMObject *modem, struct command *cmd) {
	MMModem *object;
	const gchar *power_state;
	MMModemPowerState enum_power_state;

	object = NULL;
	power_state = NULL;
	enum_power_state = MM_MODEM_POWER_STATE_UNKNOWN;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	enum_power_state = mm_modem_get_power_state(object);
	power_state = VALIDATE_UNKNOWN(mm_modem_power_state_get_string(enum_power_state));

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, power_state);

	g_object_unref(object);
	return;
}

gchar *mm_common_build_capabilities_string (const MMModemCapability *capabilities, guint n_capabilities);
void agh_modem_get_supported_capabilities(MMObject *modem, struct command *cmd) {
	gchar *caps_str;
	MMModem *object;
	MMModemCapability *caps;
	guint n_caps;

	object = NULL;
	caps = NULL;
	caps_str = NULL;
	n_caps = 0;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_supported_capabilities(object, &caps, &n_caps)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		return; /* caps array should not be set in this case, so we are not freeing it here */
	}

	caps_str = agh_mm_common_build_capabilities_string(caps, n_caps);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, caps_str);

	g_free(caps_str);
	g_free(caps);
	return;
}

void agh_modem_get_current_capabilities(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, VALIDATE_UNKNOWN(mm_modem_capability_build_string_from_mask(mm_modem_get_current_capabilities(object))));

	return;
}

void agh_modem_get_manifacturer(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_manufacturer(object)));

	return;
}

void agh_modem_get_model(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_model(object)));

	return;
}

void agh_modem_get_revision(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_revision(object)));

	return;
}

void agh_modem_get_hw_revision(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_hardware_revision(object)));

	return;
}

void agh_modem_get_drivers(MMObject *modem, struct command *cmd) {
	MMModem *object;
	gchar *mm_drivers;

	object = NULL;
	mm_drivers = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	mm_drivers = g_strjoinv (", ", (gchar **)mm_modem_get_drivers(object));

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, VALIDATE_UNKNOWN(mm_drivers));

	/* mm_drivers should become part of the restextparts queue, so we are not freeing it. */

	return;
}

void agh_modem_get_plugin(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_plugin(object)));

	return;
}

void agh_modem_get_primary_port(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_primary_port(object)));

	return;
}

void agh_modem_get_ports(MMObject *modem, struct command *cmd) {
	MMModem *object;
	MMModemPortInfo *ports;
	gchar *ports_str;
	guint n_ports;

	object = NULL;
	ports = NULL;
	n_ports = 0;
	ports_str = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_ports(object, &ports, &n_ports)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		return;
	}

	ports_str = agh_mm_common_build_ports_string(ports, n_ports);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(ports_str));

	mm_modem_port_info_array_free(ports, n_ports);
	g_free(ports_str);
	return;
}

void agh_modem_get_device(MMObject *modem, struct command *cmd) {
	MMModem *object;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, VALIDATE_UNKNOWN(mm_modem_get_device(object)));

	return;
}
