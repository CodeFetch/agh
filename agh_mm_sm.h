#ifndef __agh_mm_sm_h__
#define __agh_mm_sm_h__
#include "agh.h"
#include "agh_modem.h"
#include "agh_mm_helpers_sm.h"

void agh_mm_sm_start(struct agh_state *mstate, MMObject *m);
void agh_mm_sm_device_added(MMManager *manager, MMObject  *modem, gpointer user_data);
void agh_mm_sm_device_removed(MMManager *manager, MMObject  *modem, gpointer user_data);

#endif
