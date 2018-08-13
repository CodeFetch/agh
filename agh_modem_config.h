#ifndef __agh_modem_config_h__
#define __agh_modem_config_h__

/* config package name */
#define AGH_MODEM_UCI_CONFIG_PACKAGE "agh_modem"

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

/* config options for AGH_MM_SECTION_MODEM. */
#define AGH_MM_SECTION_MODEM_OPTION_IMEI "IMEI"
#define AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID "Equipment_Identifier"
#define AGH_MM_SECTION_MODEM_OPTION_ENABLE "enabled"
#define AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME "expected_simcards"
#define AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST "modem_bearers"
#define AGH_MM_SECTION_MODEM_OPTION_DEFAULT_BEARER "modem_default_bearer"
/* End of config options for AGH_MM_SECTION_MODEM. */

/* Config options for AGH_MM_SECTION_SIMCARD. */
#define AGH_MM_SECTION_SIMCARD_OPTION_ICCID "ICCID"
#define AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST "sim_bearers"
#define AGH_MM_SECTION_SIMCARD_OPTION_DEFAULT_BEARER "sim_default_bearer"
/* End of config options for AGH_MM_SECTION_SIMCARD. */

/* Config options for AGH_MM_SECTION_BEARER. */
#define AGH_MM_SECTION_BEARER_OPTION_USERNAME "username"
#define AGH_MM_SECTION_BEARER_OPTION_PASSWORD "password"
#define AGH_MM_SECTION_BEARER_OPTION_APN "APN"
#define AGH_MM_SECTION_BEARER_OPTION_IP_TYPE "IP_type"
#define AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD "auth_method"
/* End of config option for AGH_MM_SECTION_BEARER. */

/* Config validation errors. */

/* General failures / issues */
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM 1
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM_DESC "Out of memory while allocating UCI context"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG 2
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG_DESC "Can not find "AGH_MODEM_UCI_CONFIG_PACKAGE" config package"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION 3
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION_DESC "Invalid section type"
#define AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR 23
#define AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR_DESC "Programming error"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND 24
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND_DESC "Referenced element was not found in config"
#define AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION 25
#define AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION_DESC "This element has been referenced more than once"
#define AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED 26
#define AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED_DESC "Maximum number of bearers exceeded"

/* AGH_MM_SECTION_MODEM section related issues */
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_IMEI_OR_EQUIPMENT_ID_NOT_SPECIFIED 10
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_IMEI_OR_EQUIPMENT_ID_NOT_SPECIFIED_DESC "IMEI or Equipment ID not specified, modem can not be identified"
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY 20
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY_DESC "modem bearers GQueue was created, but is empty; this should not happen, but it did"
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_DEFAULT_BEARER_MISSING 21
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_DEFAULT_BEARER_MISSING_DESC "more than one bearer is defined for this modem, but no default one was specified"
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER 22
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER_DESC "Can not find specified default bearer amongst those listed for this modem"

/* AGH_MM_SECTION_SIMCARD issues */
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_ICCID_NOT_SPECIFIED 11
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_ICCID_NOT_SPECIFIED_DESC "SIM ICCID not found"
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY 12
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY_DESC "SIM bearers GQueue was created, but is empty; this should not happen, but it did"
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_DEFAULT_BEARER_MISSING 13
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_DEFAULT_BEARER_MISSING_DESC "more than one bearer defined for this SIM, but no default one specified"
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER 14
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER_DESC "Can not find specified default bearer amongst those defined for this SIM"

/* AGH_MM_SECTION_BEARER issues */
#define AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND 15
#define AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND_DESC "APN or authentication method not specified for this bearer"
/* End of config validation errors. */

struct agh_modem_config_validation_error {
	gchar *element_name;
	gint error_code;
	gchar *error_desc;
};

void agh_modem_validate_config(gchar *package_name, struct agh_modem_config_validation_error **validation_error);
gint agh_modem_validate_config_modem_section(struct uci_section *s, GQueue **referenced_sims, GQueue **referenced_modem_bearers);
gint agh_modem_validate_config_simcard_section(struct uci_section *s, GQueue **referenced_bearers);
gint agh_modem_validate_config_bearer_section(struct uci_section *s);
gchar *agh_modem_validate_config_strerror(gint retval);

struct agh_modem_config_validation_error *agh_modem_config_validation_error_alloc(void);
void agh_modem_config_validation_error_free(struct agh_modem_config_validation_error *e);
gint agh_modem_validate_config_check_references(struct uci_context *ctx, struct uci_package *p, GQueue *names, gchar **current_section);

#endif