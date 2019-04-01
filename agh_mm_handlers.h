#ifndef __agh_mm_handlers_h__
#define __agh_mm_handlers_h__
#include "agh_commands.h"

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

/* General subcommands "stuff" goes here. */

/* Flags. */
#define AGH_MM_FLAG_SIGNAL_QUALITY_IS_RECENT "is_recent"
/* End of flags. */

gpointer agh_mm_cmd_handle(gpointer data, gpointer hmessage);

#endif
