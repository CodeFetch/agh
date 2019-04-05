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

	/* MM Objects, used in agh_mm_handler */
	MMObject *mmobject;
	MMModem *modem;
	MMModem3gpp *modem3gpp;
	MMModem3gppUssd *modem3gppussd;
	MMModemLocation *modemlocation;
	MMModemMessaging *messaging;
	MMModemTime *time;
	MMModemFirmware *firmware;
	MMModemOma *oma;
	MMModemSignal *signal;
	MMModemVoice *voice;

	/* current command, used in agh_mm_handler (async calls) */
	struct agh_cmd *current_cmd;
	MMSim *sim;
	GList *smslist;
};

gint agh_mm_init(struct agh_state *mstate);
gint agh_mm_deinit(struct agh_state *mstate);
gint agh_modem_report_gerror_message(GError **error, struct agh_comm *comm);
gint agh_mm_report_event(struct agh_comm *comm, const gchar *evname, gchar *evpath, const gchar *evtext);
gint agh_mm_report_sms(struct agh_comm *comm, MMSms *sms);

void agh_mm_testwait(gint secs);

#endif
