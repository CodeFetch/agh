#ifndef __agh_mm_manager_h__
#define __agh_mm_manager_h__
#include <glib.h>
#include <libmm-glib.h>
#include "agh.h"

void agh_mm_manager_init(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data);
void agh_mm_manager_deinit(GDBusConnection *connection, const gchar *name, gpointer user_data);

#endif
