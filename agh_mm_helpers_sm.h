#ifndef __agh_mm_helpers_sm_h__
#define __agh_mm_helpers_sm_h__
#include "agh.h"
#include "agh_modem.h"
#include "agh_modem_config.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handlers.h"
#include "agh_ubus.h"

void agh_mm_report_failed_reason(struct agh_state *mstate, MMModem *modem);
void agh_mm_report_locked_reason(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_sim_unlock(struct agh_state *mstate, MMModem *modem, MMModemLock l);
void agh_mm_sm_sim_unlock_sim_for_pin_send_ready(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
void agh_mm_sm_report_error(struct agh_state *mstate, gchar *message);
struct uci_section *agh_mm_sm_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim);
struct uci_section *agh_mm_sm_get_modem_section(struct agh_state *mstate, MMModem *modem);
GList *agh_mm_sm_build_simlist(struct agh_state *mstate, struct uci_section *section);
void agh_mm_sm_sim_unlock_send_pin_res(MMSim *sim, GAsyncResult *res, struct agh_state *mstate);
void agh_mm_sm_report_failure_modem(struct agh_state *mstate, MMModem *modem, gchar *mmarker);
GList *agh_mm_sm_get_referenced_sections(struct agh_state *mstate, struct uci_section *section, gchar *section_name);
struct uci_section *agh_mm_sm_get_default_bearer(struct agh_state *mstate);
void agh_mm_sm_build_bearer(struct agh_state *mstate, MMModem *modem, struct uci_section *s);
void agh_mm_sm_report(struct agh_state *mstate, guint status, gchar *eventname, gchar *mmarker, gchar *name, gchar *reason, gboolean is_data);
gint agh_mm_sm_build_bearer_set_iptype(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_apn(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_auth_method(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_user(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_pass(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_roaming_allowed(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_number(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
gint agh_mm_sm_build_bearer_set_rm_protocol(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
void agh_mm_sm_connect_bearer(MMModem *modem, GAsyncResult *res, gpointer user_data);;
void agh_mm_sm_bearer_connected_changed(MMBearer *b, GParamSpec *pspec, gpointer user_data);
void agh_mm_sm_bearer_connected_notify_outside(MMBearer *b, GParamSpec *pspec, gpointer user_data);
void agh_mm_sm_keep_trying_to_connect(MMBearer *b, GAsyncResult *res, gpointer user_data);
void agh_mm_sm_keep_trying_to_connect_from_signal(MMBearer *b, GAsyncResult *res, gpointer user_data);
void agh_mm_sm_general_init(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_apply_general_modem_defaults(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_general_init_propschanges(struct agh_state *mstate, struct uci_option *opt, MMModem *modem);
void agh_mm_sm_properties_changed(MMModem *modem, GVariant *changed_props, GStrv inv_props, gpointer user_data);
void agh_mm_sm_call_outside_helper(struct agh_state *mstate, MMBearer *b);
gchar *agh_mm_sm_call_outside_build_message(struct agh_state *mstate, MMBearer *b);
gchar *agh_mm_sm_call_outside_build_message_add_element(const gchar *name, const gchar *value, gboolean last);
struct uci_section *agh_mm_sm_select_generic(struct agh_state *mstate, MMModem *modem, MMSim *sim);

#endif
