#ifndef __agh_modem_config_h__
#define __agh_modem_config_h__
#include <uci.h>

struct agh_modem_config_validation_error {
	gchar *element_name;
	gint error_code;
	gchar *error_desc;
};

void agh_modem_validate_config(struct agh_state *mstate, gchar *package_name, struct agh_modem_config_validation_error **validation_error);
void agh_modem_config_validation_error_free(struct agh_modem_config_validation_error *e);

#endif
