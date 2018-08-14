#ifndef __agh_modem_h__
#define __agh_modem_h__

#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#include "agh.h"

/* errors. */
#define AGH_MM_ERROR_NO_DBUS_CONNECTION 1
#define AGH_MM_ERROR_NO_MANAGER_OBJECT 2
#define AGH_MM_ERROR_NO_MM_PROCESS 3

/* Not really an error. */
#define AGH_MM_ERROR_DEINIT 5

#define AGH_MM_ERROR_NO_WATCH_ID 7
/* End of errors. */

/* Thanks Aleksander! Name changed to avoid potential collisions. */
#define AGH_MM_VALIDATE_UNKNOWN(str) (str ? str : "unknown")

/* ModemManager D-Bus name */
#define AGH_MM_MM_DBUS_NAME "org.freedesktop.ModemManager1"

struct agh_mm_state {
	GError *gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;
	gchar *name_owner;
	guint watch_id;
};

void agh_mm_freemem(struct agh_mm_state *mmstate, gint error);

void agh_mm_handlers_setup_ext(struct agh_state *mstate);

void agh_mm_modem_properties_changed(GDBusProxy *manager, GVariant *changed_props, GStrv inv_props, gpointer user_data);

void agh_mm_init(struct agh_state *mstate);
void agh_mm_deinit(struct agh_state *mstate);

#endif
