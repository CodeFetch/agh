#ifndef __modem_h__
#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#define __modem_h__

/* errors. */
#define AGH_MM_NO_DBUS_CONNECTION 1
#define AGH_MM_NO_MANAGER_OBJECT 2
#define AGH_MM_NO_MM_PROCESS 3
#define AGH_MM_NO_MM_LIST 4
#define AGH_MM_DEINIT 5
/* End of errors. */

/* Thanks Aleksander! */
#define VALIDATE_UNKNOWN(str) (str ? str : "unknown")

/* ModemManager D-Bus name */
#define MM_DBUS_NAME "org.freedesktop.ModemManager1"

gpointer modem_thread_start(gpointer data);
void modem_thread_deinit(gpointer data);

struct modem_async_state {
	struct command *acmd;
	MMObject *mmobject;
	GList *agh_modem_enable_all_state_list;
	guint agh_modem_enable_all_state;
};

struct modem_state {
	GError *gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;
	gchar *name_owner;
	guint mm_watch_id;

	/* modems we where not able to enable */
	GList *disabled_modems;

	/* async state */
	gboolean async_pending;
	struct modem_async_state *astate;

	/* Are we ready to accept commands? */
	gboolean ready;
	gboolean no_modems;
};

void agh_mm_freemem(struct modem_state *mmstate, gint error);

void modem_manager_init(GDBusConnection *connection, GAsyncResult *res, struct agh_thread *ct);

void modem_handlers_setup_ext(struct agh_thread *ct);
void agh_modem_enable_all(struct modem_state *mmstate);
void agh_modem_enable_next(MMModem *modem, GAsyncResult *res, struct modem_state *mmstate);
MMModem *agh_modem_enable_process_list(struct modem_state *mmstate, GList **state_list, guint *state);
void modem_free_asyncstate(struct modem_state *mstate);
void modem_new_asyncstate(struct modem_state *mstate);

/* MM comes, and goes */
void agh_mm_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data);
void agh_mm_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data);

#endif
