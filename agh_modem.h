#ifndef __agh_modem_h__
#define __agh_modem_h__

#include <glib.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#include "agh.h"

/* ModemManager D-Bus name. */
#define AGH_MM_ModemManager_DBUS_NAME "org.freedesktop.ModemManager1"

/* Log messages from AGH_LOG_DOMAIN_MODEM domain. */
#define AGH_LOG_DOMAIN_MODEM "MM"

/* Logging macros. */
#define agh_log_mm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)
#define agh_log_mm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)

struct agh_mm_state {
	GError *current_gerror;
	GDBusConnection *dbus_connection;
	MMManager *manager;
	guint watch_id;
	gulong manager_signal_modem_added_id;
	gulong manager_signal_modem_removed_id;
	struct uci_context *mctx;
	struct uci_package *uci_package;
};

gint agh_mm_init(struct agh_state *mstate);
gint agh_mm_deinit(struct agh_state *mstate);
gint agh_modem_report_gerror_message(GError **error);
gint agh_mm_report_event(struct agh_state *mstate, const gchar *evname, const gchar *evtext);

void agh_mm_testwait(gint secs);

#endif
