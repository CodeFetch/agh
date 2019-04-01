#ifndef __agh_modem_h__
#define __agh_modem_h__

#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#include "agh.h"

/* ModemManager D-Bus name. */
#define AGH_MM_ModemManager_DBUS_NAME "org.freedesktop.ModemManager1"

/* modem event name */
#define AGH_MM_MODEM_EVENT_NAME "modem"

struct agh_mm_state {
	GError *current_gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;
	guint watch_id;
	gulong manager_signal_modem_added_id;
	gulong manager_signal_modem_removed_id;
	struct uci_context *mctx;
	struct uci_package *uci_package;
	GSource *bearers_check;
	guint bearers_check_tag;
	gboolean global_bearer_connecting_lock;
};

gint agh_mm_init(struct agh_state *mstate);
gint agh_mm_deinit(struct agh_state *mstate);
gint agh_modem_report_gerror_message(GError **error);
gint agh_mm_report_event(struct agh_state *mstate, const gchar *evname, gchar *evpath, const gchar *evtext);

void agh_mm_testwait(gint secs);

#endif
