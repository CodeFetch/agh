#ifndef __agh_modem_config_h__
#define __agh_modem_config_h__
#include <uci.h>

gint agh_modem_validate_config(struct agh_mm_state *mmstate, gchar *package_name);

/* Functions useful to search for a config section related to a given MM object. */
struct uci_section *agh_mm_config_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim);
struct uci_section *agh_mm_config_get_modem_section(struct agh_state *mstate, MMModem *modem);

#endif
