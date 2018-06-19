#ifndef __modem_h__
#include <glib.h>
#include <gio/gio.h>
#define __modem_h__

void modem_thread_init(gpointer data);
gpointer modem_thread_start(gpointer data);
void modem_thread_deinit(gpointer data);

static struct agh_thread modem_thread_ops = {
	.thread_name = "Modem",
	.agh_thread_init = modem_thread_init,
	.agh_thread_main = modem_thread_start,
	.agh_thread_deinit = modem_thread_deinit,
	.on_stack = 1
};

struct modem_state {
	GError *error;
	GDBusConnection *dbus_connection;
};

#endif
