#include "agh_mm_manager.h"

void agh_mm_manager_init(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data) {
	g_print("Appeared.\n");
}

void agh_mm_manager_deinit(GDBusConnection *connection, const gchar *name, gpointer user_data) {
	g_print("Disappeared.\n");
}
