#ifndef __agh_mm_helpers_sm_h__
#define __agh_mm_helpers_sm_h__
#include "agh.h"
#include "agh_modem.h"
#include "agh_modem_config.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handlers.h"
#include "agh_ubus.h"

void agh_mm_sm_report_failure_modem(struct agh_state *mstate, MMModem *modem, gchar *mmarker);
void agh_mm_report_failed_reason(struct agh_state *mstate, MMModem *modem);
void agh_mm_report_locked_reason(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_sim_unlock(struct agh_state *mstate, MMModem *modem, MMModemLock l);
void agh_mm_sm_general_init(struct agh_state *mstate, MMModem *modem);
struct uci_section *agh_mm_sm_get_modem_section(struct agh_state *mstate, MMModem *modem);
struct uci_section *agh_mm_sm_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim);
GList *agh_mm_sm_get_referenced_sections(struct agh_state *mstate, struct uci_section *section, gchar *section_name);
struct uci_section *agh_mm_sm_get_default_bearer(struct agh_state *mstate);
void agh_mm_sm_build_bearer(struct agh_state *mstate, MMModem *modem, struct uci_section *s);
struct uci_section *agh_mm_sm_select_generic(struct agh_state *mstate, MMModem *modem, MMSim *sim);

#endif
