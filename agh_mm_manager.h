#ifndef __agh_mm_manager_h__
#define __agh_mm_manager_h__
#include <glib.h>
#include <libmm-glib.h>
#include "agh.h"
#include "agh_modem.h"

void agh_mm_manager_init(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data);
void agh_mm_manager_init_finish(GDBusConnection *connection, GAsyncResult *res, struct agh_state *mstate);

void agh_mm_manager_deinit(GDBusConnection *connection, const gchar *name, gpointer user_data);

/* Some manager signals. */
void agh_mm_manager_signals_init(struct agh_state *mstate);
void agh_mm_manager_signals_deinit(struct agh_state *mstate);

#endif
