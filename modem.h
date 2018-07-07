#ifndef __modem_h__
#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#define __modem_h__

/* agh_mm_freemem errors */
#define AGH_MM_NO_DBUS_CONNECTION 1
#define AGH_MM_NO_MANAGER_OBJECT 2
#define AGH_MM_NO_MM_PROCESS 3
#define AGH_MM_DEINIT 4
/* end of agh_mm_freemem errors */

/* Thanks Aleksander! */
#define VALIDATE_UNKNOWN(str) (str ? str : "unknown")

/* ModemManager D-Bus name */
#define MM_DBUS_NAME "org.freedesktop.ModemManager1"

void modem_thread_init(gpointer data);
gpointer modem_thread_start(gpointer data);
void modem_thread_deinit(gpointer data);

struct modem_async_state {
	struct command *acmd;
	MMObject *mmobject;
};

struct modem_state {
	GError *gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;

	/* async state */
	gboolean async_pending;
	struct modem_async_state *astate;
};

void agh_mm_freemem(struct modem_state *mmstate, gint error);

void modem_manager_init(GDBusConnection *connection, GAsyncResult *res, struct agh_thread *ct);

void modem_handlers_setup_ext(struct agh_thread *ct);
void modem_free_asyncstate(struct modem_state *mstate);
void modem_new_asyncstate(struct modem_state *mstate);

/* MM comes, and goes */

#endif
