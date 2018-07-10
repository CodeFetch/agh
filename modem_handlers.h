#ifndef __modem_handlers_h__
#define __modem_handlers_h__

/* String error responses. */
#define AGH_MM_MSG_DATA_NOT_AVAILABLE "NOT_AVAILABLE"
#define AGH_MM_MSG_IMEI_NOT_AVAILABLE "IMEI_NOT_AVAILABLE"
#define AGH_NO_MMModem3gpp_OBJECT "NO_MMModem3gpp_OBJECT"
#define AGH_NO_MMModem_OBJECT "NO_MMModem_OBJECT"
#define AGH_MM_INVALID_SUBCOMMAND "MM_INVALID_SUBCOMMAND"
#define AGH_MM_NO_MM_PROCESS_TEXT "MM_NOT_RUNNING"
#define AGH_MM_NOT_READY "NOT_READY_OR_BUG"
#define AGH_MM_NO_MODEMS "NO_MODEMS"
#define AGH_MM_INVALID_MODEM "MM_INVALID_MODEM"
/* End of string error responses. */

/* Operations */
#define AGH_CMD_MODEM	"modem"

/* Subcommands for a specific modem. */
#define AGH_CMD_MM_GET_IMEI "get_imei"
#define AGH_CMD_MM_GET_STATE "get_state"
#define AGH_CMD_MM_GET_POWER_STATE "get_power_state"
#define AGH_CMD_MM_GET_SUPPORTED_CAPABILITIES "get_supported_capabilities"
#define AGH_CMD_MM_GET_CURRENT_CAPABILITIES "get_current_capabilities"
#define AGH_CMD_MM_GET_MANIFACTURER "get_manifacturer"
#define AGH_CMD_MM_GET_MODEL "get_model"
#define AGH_CMD_MM_GET_REVISION "get_rev"
#define AGH_CMD_MM_GET_HARDWARE_REVISION "get_hwrev"
#define AGH_CMD_MM_GET_DRIVERS "drivers"
#define AGH_CMD_MM_GET_PLUGIN "get_ModemManager_plugin"
#define AGH_CMD_MM_GET_PRIMARY_PORT "get_primary_port"
#define AGH_CMD_MM_GET_PORTS "get_ports"
#define AGH_CMD_MM_GET_DEVICE "get_device"
#define AGH_CMD_MM_GET_EQUIPMENT_IDENTIFIER "get_equipment_identifier"
#define AGH_CMD_MM_GET_DEVICE_IDENTIFIER "get_deviceid"
#define AGH_CMD_MM_GET_UNLOCK_REQUIRED "get_lockstatus"
#define AGH_CMD_MM_GET_UNLOCK_RETRIES "get_unlock_retries"
#define AGH_CMD_MM_GET_MAX_BEARERS "max_bearers"
#define AGH_CMD_MM_GET_MAX_ACTIVE_BEARERS "max_active_bearers"
#define AGH_CMD_MM_GET_OWN_NUMBERS "get_own_numbers"
#define AGH_CMD_MM_GET_SUPPORTED_MODES "supported_modes"
#define AGH_CMD_MM_GET_CURRENT_MODES "current_modes"
#define AGH_CMD_MM_GET_SUPPORTED_BANDS "supported_bands"
#define AGH_CMD_MM_GET_CURRENT_BANDS "current_bands"
#define AGH_CMD_MM_GET_SUPPORTED_IP_FAMILIES "ip_families"
#define AGH_CMD_MM_GET_SIGNAL_QUALITY "signal_quality"
#define AGH_CMD_MM_GET_ACCESS_TECHNOLOGIES "access_technologies"
/* end of subcommands for a specific modem. */

/* General subcommands. */
#define AGH_CMD_MM_LIST_DISABLED_MODEMS "list_disabled_modems"
/* End of general subcommands. */

/* Flags. */
#define AGH_MM_FLAG_SIGNAL_QUALITY_IS_RECENT "is_recent"
/* End of flags. */

gpointer modem_cmd_handle(gpointer data, gpointer hmessage);

void agh_mm_list_modems(struct modem_state *mmstate, struct command *cmd);
void agh_mm_list_modem_single(gpointer data, gpointer user_data);
gchar *agh_mm_modem_to_index(const gchar *modem_path);
MMObject *agh_mm_index_to_modem(struct modem_state *mmstate, gint modem_index);
void agh_modem_do(MMObject *modem, struct command *cmd);

/* MMModem3gpp */
MMModem3gpp *agh_get_MMModem3gpp_object(MMObject *modem, struct command *cmd);
void agh_modem_get_imei(MMObject *modem, struct command *cmd);

/* MMModem */
MMModem *agh_get_MMModem_object(MMObject *modem, struct command *cmd);
void agh_modem_get_state(MMObject *modem, struct command *cmd);
void agh_modem_get_power_state(MMObject *modem, struct command *cmd);
void agh_modem_get_supported_capabilities(MMObject *modem, struct command *cmd);
void agh_modem_get_current_capabilities(MMObject *modem, struct command *cmd);
void agh_modem_get_manifacturer(MMObject *modem, struct command *cmd);
void agh_modem_get_model(MMObject *modem, struct command *cmd);
void agh_modem_get_revision(MMObject *modem, struct command *cmd);
void agh_modem_get_hw_revision(MMObject *modem, struct command *cmd);
void agh_modem_get_drivers(MMObject *modem, struct command *cmd);
void agh_modem_get_plugin(MMObject *modem, struct command *cmd);
void agh_modem_get_primary_port(MMObject *modem, struct command *cmd);
void agh_modem_get_ports(MMObject *modem, struct command *cmd);
void agh_modem_get_device(MMObject *modem, struct command *cmd);
void agh_modem_get_equipment_identifier(MMObject *modem, struct command *cmd);
void agh_modem_get_device_identifier(MMObject *modem, struct command *cmd);
void agh_modem_get_unlock_required(MMObject *modem, struct command *cmd);
void agh_modem_get_unlock_retries(MMObject *modem, struct command *cmd);
void agh_modem_get_max_bearers(MMObject *modem, struct command *cmd);
void agh_modem_get_max_active_bearers(MMObject *modem, struct command *cmd);
void agh_modem_get_own_numbers(MMObject *modem, struct command *cmd);
void agh_modem_get_supported_modes(MMObject *modem, struct command *cmd);
void agh_modem_get_current_modes(MMObject *modem, struct command *cmd);
void agh_modem_get_supported_bands(MMObject *modem, struct command *cmd);
void agh_modem_get_current_bands(MMObject *modem, struct command *cmd);
void agh_modem_get_supported_IP_families(MMObject *modem, struct command *cmd);
void agh_modem_get_signal_quality(MMObject *modem, struct command *cmd);
void agh_modem_get_access_technologies(MMObject *modem, struct command *cmd);
void agh_mm_list_disabled_modems(struct modem_state *mmstate, struct command *cmd);

#endif
