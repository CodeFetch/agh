#ifndef __agh_mm_sm_h__
#define __agh_mm_sm_h__
#include "agh.h"
#include "agh_modem.h"
#include "agh_mm_helpers_sm.h"

void agh_mm_sm_start(struct agh_state *mstate, MMObject *m);
void agh_mm_sm_statechange(MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason, gpointer user_data);

void agh_mm_sm_stateaction(struct agh_state *mstate, MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason);

void agh_mm_sm_device_added(MMManager *manager, MMObject  *modem, gpointer user_data);
void agh_mm_sm_device_removed(MMManager *manager, MMObject  *modem, gpointer user_data);
void agh_mm_sm_unlock(struct agh_state *mstate, MMModem *modem);

void agh_mm_sm_enable(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_enable_finish(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
void agh_mm_sm_bearers_init(struct agh_state *mstate, MMModem *modem);
void agh_mm_sm_bearers_init_get_list(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
void agh_mm_sm_bearers_delete(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
void agh_mm_sm_bearers_delete_next(MMModem *modem, GAsyncResult *res, struct agh_mm_asyncstate *a);
void agh_mm_sm_handle_cfg_bearers_list(struct agh_state *mstate, MMModem *modem, GList *blist);

#endif
