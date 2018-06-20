#ifndef __modem_h__
#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#define __modem_h__

void modem_thread_init(gpointer data);
gpointer modem_thread_start(gpointer data);
void modem_thread_deinit(gpointer data);

struct modem_state {
	GError *gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;
	gchar *name_owner;
};

void mm_freebase(struct modem_state *mmstate);

void modem_manager_init(GDBusConnection *connection, GAsyncResult *res, struct agh_thread *ct);

#endif
