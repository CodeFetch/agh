#ifndef __agh_modem_config_h__
#define __agh_modem_config_h__
#include <uci.h>

/* config section types */
#define AGH_MM_SECTION_UNKNOWN 0
#define AGH_MM_SECTION_MODEM_NAME "modem"
#define AGH_MM_SECTION_MODEM 1
#define AGH_MM_SECTION_SIMCARD_NAME "simcard"
#define AGH_MM_SECTION_SIMCARD 2
#define AGH_MM_SECTION_BEARER_NAME "bearer"
#define AGH_MM_SECTION_BEARER 3
/* end of config section types */

/* Maximum number of bearers accepted in a AGH_MM_SECTION_SIMCARD section */
#define AGH_MM_SECTION_SIMCARD_MAXBEARERS 2

/* or in a modem section */
#define AGH_MM_SECTION_MODEM_MAXBEARERS 2

/* maximum number of SIM cards specified in a modem section */
#define AGH_MM_SECTION_MODEM_MAXSIMS 4

/* config options for AGH_MM_SECTION_MODEM. */
#define AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID "Equipment_Identifier"
#define AGH_MM_SECTION_MODEM_OPTION_ENABLE "enable"
#define AGH_MM_SECTION_MODEM_OPTION_CONNECT "connect"
#define AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME "expected_simcards"
#define AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST "modem_bearers"
#define AGH_MM_SECTION_MODEM_OPTION_REPORT_PROPSCHANGES "report_changes"
/* End of config options for AGH_MM_SECTION_MODEM. */

/* Config options for AGH_MM_SECTION_SIMCARD. */
#define AGH_MM_SECTION_SIMCARD_OPTION_SIM_ID "id"
#define AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST "sim_bearers"
#define AGH_MM_SECTION_SIMCARD_OPTION_PIN_CODE "PIN_code"
/* End of config options for AGH_MM_SECTION_SIMCARD. */

/* Config options for AGH_MM_SECTION_BEARER. */
#define AGH_MM_SECTION_BEARER_OPTION_USERNAME "user"
#define AGH_MM_SECTION_BEARER_OPTION_PASSWORD "password"
#define AGH_MM_SECTION_BEARER_OPTION_APN "apn"
#define AGH_MM_SECTION_BEARER_OPTION_IP_TYPE "ip_type"
#define AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD "allowed_auth"
#define AGH_MM_SECTION_BEARER_OPTION_NUMBER "number"
#define AGH_MM_SECTION_BEARER_OPTION_ALLOW_ROAMING "allow_roaming"
#define AGH_MM_SECTION_BEARER_OPTION_RM_PROTOCOL "rm_protocol"
#define AGH_MM_SECTION_BEARER_OPTION_OPERATOR_ID "operator_id"
/* End of config option for AGH_MM_SECTION_BEARER. */

/* Config validation errors. */

/* General failures / issues */
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM 1
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG 2
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION 3
#define AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR 23
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND 24
#define AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION 25
#define AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED 26
#define AGH_MODEM_VALIDATE_CONFIG_SET_CONFDIR_FAILURE 27

/* AGH_MM_SECTION_MODEM section related issues */
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED 10
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY 20
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_SIMLIST_GQUEUE_PRESENT_BUT_EMPTY 21
#define AGH_MODEM_VALIDATE_CONFIG_MAXSIMS_EXCEEDED 22

/* AGH_MM_SECTION_SIMCARD issues */
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED 11
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY 12

/* AGH_MM_SECTION_BEARER issues */
#define AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTH_METHOD_NOT_FOUND 15
/* End of config validation errors. */

gint agh_modem_validate_config(struct agh_mm_state *mmstate, const gchar *path, gchar *package_name);

/* Functions useful to search for a config section related to a given MM object. */
struct uci_section *agh_mm_config_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim);
struct uci_section *agh_mm_config_get_modem_section(struct agh_state *mstate, MMModem *modem);
gint agh_mm_config_get_boolean(struct uci_option *o);
GList *agh_mm_config_get_referenced_sections(struct agh_state *mstate, struct uci_section *section, gchar *section_name);
gint agh_mm_config_build_bearer(struct agh_state *mstate, MMModem *modem, struct uci_section *s, GAsyncReadyCallback cb);
struct uci_section *agh_mm_get_default_bearer(struct agh_state *mstate);

struct agh_mm_config_build_bearer_ctx *agh_mm_config_build_bearer_ctx_init(struct agh_state *mstate, struct uci_section *section);
gint agh_mm_config_build_bearer_ctx_deinit(struct agh_mm_config_build_bearer_ctx *ctx);
gint agh_mm_config_build_bearer_ctx_bearer(struct agh_mm_config_build_bearer_ctx *ctx, MMBearer *b);

#endif
