#ifndef __modem_h__
#include <glib.h>
#include <gio/gio.h>
#define __modem_h__

void modem_thread_init(gpointer data);
gpointer modem_thread_start(gpointer data);
void modem_thread_deinit(gpointer data);

struct modem_state {
	GError *error;
	GDBusConnection *dbus_connection;
};

#endif
