#include "agh_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_modem.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handlers.h"

/* Function prototypes. */
static void agh_mm_list_modems(struct agh_mm_state *mmstate, struct agh_cmd *cmd);
static void agh_mm_list_modem_single(gpointer data, gpointer user_data);
static MMObject *agh_mm_index_to_modem(struct agh_mm_state *mmstate, gint modem_index);
static void agh_modem_do(MMObject *modem, struct agh_cmd *cmd);

/* MMModem3gpp */
static MMModem3gpp *agh_get_MMModem3gpp_object(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_imei(MMObject *modem, struct agh_cmd *cmd);

/* MMModem */
static MMModem *agh_get_MMModem_object(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_state(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_power_state(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_supported_capabilities(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_current_capabilities(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_manifacturer(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_model(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_revision(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_hw_revision(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_drivers(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_plugin(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_primary_port(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_ports(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_device(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_equipment_identifier(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_device_identifier(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_unlock_required(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_unlock_retries(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_max_bearers(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_max_active_bearers(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_own_numbers(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_supported_modes(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_current_modes(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_supported_bands(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_current_bands(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_supported_IP_families(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_signal_quality(MMObject *modem, struct agh_cmd *cmd);
static void agh_modem_get_access_technologies(MMObject *modem, struct agh_cmd *cmd);

gpointer agh_mm_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_state *mstate = h->handler_data;
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct agh_cmd *cmd;
	const gchar __attribute__((unused)) *string_arg;
	gint current_modem;
	config_setting_t *arg;
	struct agh_message *answer;
	MMObject *modem;
	void (*general_subcommand_cb)(struct agh_mm_state *mmstate, struct agh_cmd *cmd);

	cmd = NULL;
	string_arg = NULL;
	current_modem = 0;
	arg = NULL;
	answer = NULL;
	modem = NULL;
	general_subcommand_cb = NULL;

	if (m->msg_type != MSG_SENDCMD)
		return cmd;

	cmd = m->csp;

	/* If this is not the AGH_CMD_MODEM command, then stop here. */
	if (g_strcmp0(cmd_get_operation(cmd), AGH_CMD_MODEM))
		return NULL;

	if (mstate->exiting || !mmstate->manager)
		return NULL;

	cmd_answer_prepare(cmd);

	/* If an integer was specified, then this is the modem on which we're supposed to operate. Otherwise it's a subcommand. */
	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_INT);
	if (arg) {
		/* A modem was specified. */
		current_modem = config_setting_get_int(arg);
		modem = agh_mm_index_to_modem(mmstate, current_modem);

		if (modem) {
			//g_print("Modem %" G_GINT16_FORMAT" has been specified.\n",current_modem);
			agh_modem_do(modem, cmd);
			g_object_unref(modem);
			modem = NULL;
		} else {
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_MM_INVALID_MODEM);
		}

		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);

	if (arg) {
		string_arg = config_setting_get_string(arg);

		/* Implement general subcommands here. */

		if (!general_subcommand_cb) {
			cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
			cmd_answer_addtext(cmd, AGH_MM_INVALID_SUBCOMMAND);
		}
		else
			general_subcommand_cb(mmstate, cmd);

		answer = cmd_answer_msg(cmd, mstate->comm, NULL);
		return answer;
	}

	agh_mm_list_modems(mmstate, cmd);

	answer = cmd_answer_msg(cmd, mstate->comm, NULL);

	return answer;
}

/* Get and report a list of modems known by ModemManager. */
static void agh_mm_list_modems(struct agh_mm_state *mmstate, struct agh_cmd *cmd) {
	GList *modems;

	modems = NULL;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

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

static void agh_mm_list_modem_single(gpointer data, gpointer user_data) {
	MMObject *modem_object = data;
	struct agh_cmd *cmd = user_data;
	gchar *modem_index;

	modem_index = agh_mm_modem_to_index(mm_object_get_path(modem_object));

	cmd_answer_addtext(cmd, modem_index);
	g_free(modem_index);

	return;
}

/* Input to this function should come from MM calls, in general. That's why no validation is done.
 * And yes, the way AGH handles modem index probably needs to be done differently.
*/
static MMObject *agh_mm_index_to_modem(struct agh_mm_state *mmstate, gint modem_index) {
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

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems)
		return modem_found;

	modem_list_length = g_list_length(modems);

	if (!modem_list_length) {
		g_list_free(modems);
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

static void agh_modem_do(MMObject *modem, struct agh_cmd *cmd) {
	const gchar *current_operation;
	void (*cb)(MMObject *modem, struct agh_cmd *cmd);

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
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_EQUIPMENT_IDENTIFIER))
		cb = agh_modem_get_equipment_identifier;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_DEVICE_IDENTIFIER))
		cb = agh_modem_get_device_identifier;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_UNLOCK_REQUIRED))
		cb = agh_modem_get_unlock_required;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_UNLOCK_RETRIES))
		cb = agh_modem_get_unlock_retries;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_MAX_BEARERS))
		cb = agh_modem_get_max_bearers;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_MAX_ACTIVE_BEARERS))
		cb = agh_modem_get_max_active_bearers;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_OWN_NUMBERS))
		cb = agh_modem_get_own_numbers;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_SUPPORTED_MODES))
		cb = agh_modem_get_supported_modes;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_CURRENT_MODES))
		cb = agh_modem_get_current_modes;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_SUPPORTED_BANDS))
		cb = agh_modem_get_supported_bands;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_CURRENT_BANDS))
		cb = agh_modem_get_current_bands;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_SUPPORTED_IP_FAMILIES))
		cb = agh_modem_get_supported_IP_families;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_SIGNAL_QUALITY))
		cb = agh_modem_get_signal_quality;
	if (!g_strcmp0(current_operation, AGH_CMD_MM_GET_ACCESS_TECHNOLOGIES))
		cb = agh_modem_get_access_technologies;

	if (!cb) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_INVALID_SUBCOMMAND);
		return;
	}

	cb(modem, cmd);

	return;
}

static MMModem3gpp *agh_get_MMModem3gpp_object(MMObject *modem, struct agh_cmd *cmd) {
	MMModem3gpp *object;

	object = NULL;

	object = mm_object_get_modem_3gpp(modem);

	if (!object) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_NO_MMModem3gpp_OBJECT);
	}

	return object;
}

static void agh_modem_get_imei(MMObject *modem, struct agh_cmd *cmd) {
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

static MMModem *agh_get_MMModem_object(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;

	object = NULL;

	object = mm_object_get_modem(modem);

	if (!object) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_NO_MMModem_OBJECT);
	}

	return object;
}

static void agh_modem_get_state(MMObject *modem, struct agh_cmd *cmd) {
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
	state = AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_get_string(enum_state));
	enum_state_failed_reason = mm_modem_get_state_failed_reason(object);
	state_failed_reason = AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_failed_reason_get_string(enum_state_failed_reason));

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, state);
	cmd_answer_addtext(cmd, state_failed_reason);

	g_object_unref(object);
	return;
}

static void agh_modem_get_power_state(MMObject *modem, struct agh_cmd *cmd) {
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
	power_state = AGH_MM_VALIDATE_UNKNOWN(mm_modem_power_state_get_string(enum_power_state));

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, power_state);

	g_object_unref(object);
	return;
}

static void agh_modem_get_supported_capabilities(MMObject *modem, struct agh_cmd *cmd) {
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
		g_object_unref(object);
		return; /* caps array should "not be set" in this case, so we are not freeing it here */
	}

	caps_str = agh_mm_common_build_capabilities_string(caps, n_caps);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, caps_str);

	g_free(caps_str);
	g_free(caps);
	g_object_unref(object);
	return;
}

static void agh_modem_get_current_capabilities(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *current_caps;

	object = NULL;
	current_caps = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	current_caps = mm_modem_capability_build_string_from_mask(mm_modem_get_current_capabilities(object));

	if (current_caps) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, current_caps);
		g_free(current_caps);
		current_caps = NULL;
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_manifacturer(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *manifacturer;

	object = NULL;
	manifacturer = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	manifacturer = mm_modem_get_manufacturer(object);

	if (manifacturer) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, manifacturer);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_model(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *model;

	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	model = mm_modem_get_model(object);

	if (model) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, model);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_revision(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *revision;

	object = NULL;
	revision = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	revision = mm_modem_get_revision(object);

	if (revision) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, AGH_MM_VALIDATE_UNKNOWN(mm_modem_get_revision(object)));
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_hw_revision(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *hrev;

	object = NULL;
	hrev = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	hrev = mm_modem_get_hardware_revision(object);

	if (hrev) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, hrev);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_drivers(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *mm_drivers;

	object = NULL;
	mm_drivers = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	mm_drivers = g_strjoinv (", ", (gchar **)mm_modem_get_drivers(object));

	if (mm_drivers) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_peektext(cmd, mm_drivers);
	}

	/* mm_drivers should become part of the restextparts queue, so we are not freeing it. */

	g_object_unref(object);
	return;
}

static void agh_modem_get_plugin(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *current_plugin;

	object = NULL;
	current_plugin = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	current_plugin = mm_modem_get_plugin(object);

	if (current_plugin) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, current_plugin);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_primary_port(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *primary_port;

	object = NULL;
	primary_port = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	primary_port = mm_modem_get_primary_port(object);

	if (primary_port) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, primary_port);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_ports(MMObject *modem, struct agh_cmd *cmd) {
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
		g_object_unref(object);
		return;
	}

	ports_str = agh_mm_common_build_ports_string(ports, n_ports);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, ports_str);

	mm_modem_port_info_array_free(ports, n_ports);

	g_object_unref(object);
	g_free(ports_str);
	return;
}

static void agh_modem_get_device(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *sysfs_device;

	object = NULL;
	sysfs_device = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	sysfs_device = mm_modem_get_device(object);

	if (sysfs_device) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, sysfs_device);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_equipment_identifier(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *equipment_id;

	object = NULL;
	equipment_id = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	equipment_id = mm_modem_get_equipment_identifier(object);

	if (equipment_id) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, equipment_id);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_device_identifier(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *device_id;

	object = NULL;
	device_id = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	device_id = mm_modem_get_device_identifier(object);

	if (device_id) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, device_id);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_unlock_required(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	const gchar *unlock_required;

	object = NULL;
	unlock_required = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	unlock_required = mm_modem_lock_get_string(mm_modem_get_unlock_required(object));

	if (unlock_required) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, unlock_required);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_unlock_retries(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	MMUnlockRetries *retries;
	gchar *retries_str;

	object = NULL;
	retries = NULL;
	retries_str = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	retries = mm_modem_get_unlock_retries(object);

	if (!retries) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		return;
	}

	retries_str = agh_mm_unlock_retries_build_string(retries);

	if (retries_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, retries_str);
		g_free(retries_str);
	}

	g_object_unref(retries);
	g_object_unref(object);
	return;
}

static void agh_modem_get_max_bearers(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *maxbearers_str;

	object = NULL;
	maxbearers_str = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	maxbearers_str = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_modem_get_max_bearers(object));

	if (maxbearers_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_peektext(cmd, maxbearers_str);
	}

	g_object_unref(object);
	return;
}

static void agh_modem_get_max_active_bearers(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *max_active_bearers_str;

	object = NULL;
	max_active_bearers_str = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	max_active_bearers_str = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_modem_get_max_active_bearers(object));

	if (max_active_bearers_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_peektext(cmd, max_active_bearers_str);
	}

	g_object_unref(object);
	return;
}

/*
 * Some explanations are due in this case.
 * First, this function will return a space-separated lists of modem own numbers, so I guess youcan have things like:
 * CMD_OUT_KEYWORD = ( id, status, "num1, num2, ..." )
 * I guess it would be better if we did split own numbers in different arguments, but at the moment, amongst other things, I have no device / SIM to test this.
 *
 * Secondly, it would be better to remove that (gchar**) cast on mm_modem_get_own_numbers call. I don't like it, but probably I should learn C better. What do you think ?
*/
static void agh_modem_get_own_numbers(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *own_numbers;
	gchar **tmp;

	object = NULL;
	own_numbers = NULL;
	tmp = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	tmp = (gchar**)mm_modem_get_own_numbers(object);

	if (!tmp) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return;
	}

	own_numbers = g_strjoinv(", ", tmp);
	if (!own_numbers[0]) {
		g_free(own_numbers);
		own_numbers = NULL;
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return;
	}

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, own_numbers);

	g_object_unref(object);
	return;
}

static void agh_modem_get_supported_modes(MMObject *modem, struct agh_cmd *cmd) {
	gchar *modes_str;
	MMModem *object;
	MMModemModeCombination *modes;
	guint n_modes;

	object = NULL;
	modes = NULL;
	modes_str = NULL;
	n_modes = 0;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_supported_modes(object, &modes, &n_modes)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return; /* modes array should "not be set" in this case, so we are not freeing it here */
	}

	modes_str = agh_mm_common_build_mode_combinations_string(modes, n_modes);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(cmd, modes_str);

	g_free(modes_str);
	g_free(modes);
	g_object_unref(object);
	return;
}

static void agh_modem_get_current_modes(MMObject *modem, struct agh_cmd *cmd) {
	gchar *allowed_modes_str;
	MMModemMode allowed_modes;
	MMModemMode preferred_mode;
	gchar *preferred_modes_str;
	MMModem *object;

	object = NULL;
	allowed_modes_str = NULL;
	preferred_modes_str = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_current_modes(object, &allowed_modes, &preferred_mode)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return; /* relevant data on the stack for now */
	}

	allowed_modes_str = mm_modem_mode_build_string_from_mask(allowed_modes);
	preferred_modes_str = mm_modem_mode_build_string_from_mask(preferred_mode);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);

	if (allowed_modes_str) {
		cmd_answer_addtext(cmd, allowed_modes_str);
		g_free(allowed_modes_str);
	}

	if (preferred_modes_str) {
		cmd_answer_addtext(cmd, preferred_modes_str);
		g_free(preferred_modes_str);
	}

	g_object_unref(object);
	return;
}

/* Docs says that for POTS devices, only MM_MODEM_BAND_ANY will be returned in bands array. */
static void agh_modem_get_supported_bands(MMObject *modem, struct agh_cmd *cmd) {
	gchar *bands_str;
	MMModem *object;
	MMModemBand *bands;
	guint n_bands;

	object = NULL;
	bands = NULL;
	bands_str = NULL;
	n_bands = 0;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_supported_bands(object, &bands, &n_bands)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return; /* bands array should "not be set" in this case, so we are not freeing it here */
	}

	bands_str = agh_mm_common_build_bands_string(bands, n_bands);

	if (bands_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, bands_str);
		g_free(bands_str);
	}

	g_free(bands);
	g_object_unref(object);
	return;
}

/* Docs says: for POTS devices, only the MM_MODEM_BAND_ANY band is supported. */
static void agh_modem_get_current_bands(MMObject *modem, struct agh_cmd *cmd) {
	gchar *bands_str;
	MMModem *object;
	MMModemBand *bands;
	guint n_bands;

	object = NULL;
	bands = NULL;
	bands_str = NULL;
	n_bands = 0;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	if (!mm_modem_get_current_bands(object, &bands, &n_bands)) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(cmd, AGH_MM_MSG_DATA_NOT_AVAILABLE);
		g_object_unref(object);
		return; /* bands array should "not be set" in this case, so we are not freeing it here */
	}

	bands_str = agh_mm_common_build_bands_string(bands, n_bands);

	if (bands_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, bands_str);
		g_free(bands_str);
	}

	g_free(bands);
	g_object_unref(object);
	return;
}

static void agh_modem_get_supported_IP_families(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *ip_families_str;

	ip_families_str = NULL;
	object = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	ip_families_str = mm_bearer_ip_family_build_string_from_mask(mm_modem_get_supported_ip_families(object));

	if (ip_families_str) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, ip_families_str);
		g_free(ip_families_str);
	}

	g_object_unref(object);
	return;
}

/* Enable the modem for best results. */
static void agh_modem_get_signal_quality(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *sigqual_str;
	gboolean is_recent;
	guint signal_quality;

	object = NULL;
	sigqual_str = NULL;
	is_recent = FALSE;
	signal_quality = 0;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	signal_quality = mm_modem_get_signal_quality(object, &is_recent);
	sigqual_str = g_strdup_printf("%" G_GUINT16_FORMAT"",signal_quality);

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, sigqual_str);
	if (is_recent)
		cmd_answer_addtext(cmd, AGH_MM_FLAG_SIGNAL_QUALITY_IS_RECENT);

	g_object_unref(object);
	return;
}

static void agh_modem_get_access_technologies(MMObject *modem, struct agh_cmd *cmd) {
	MMModem *object;
	gchar *access_technologies;

	object = NULL;
	access_technologies = NULL;

	object = agh_get_MMModem_object(modem, cmd);

	if (!object)
		return;

	access_technologies = mm_modem_access_technology_build_string_from_mask(mm_modem_get_access_technologies(object));

	if (access_technologies) {
		cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
		cmd_answer_addtext(cmd, access_technologies);
		g_free(access_technologies);
	}

	g_object_unref(object);
	return;
}
