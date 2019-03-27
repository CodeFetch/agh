/*
 * So here we are, for the second time: after the HDD breakage.
 * Subsequently, it got rewritten: the previous one wasn't so great.
*/

#include <glib.h>
#include <libmm-glib.h>
#include "agh_modem.h"
#include "agh.h"
#include "agh_logging.h"
#include "agh_handlers.h"
#include "agh_mm_handlers.h"
#include "agh_modem_config.h"

/* Log messages from AGH_LOG_DOMAIN_MODEM domain. */
#define AGH_LOG_DOMAIN_MODEM "MM"

/* Logging macros. */
#define agh_log_mm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)
#define agh_log_mm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)

static void agh_mm_start(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data) {
	struct agh_state *mstate = user_data;
	agh_log_mm_dbg("MM (%s) is present",name);
	return;
}

static void agh_mm_stop(GDBusConnection *connection, const gchar *name, gpointer user_data) {
	struct agh_state *mstate = user_data;
	agh_log_mm_dbg("MM (%s) is no more present",name);
	return;
}

static gint agh_mm_watch_deinit(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH or MM state");
		retval = 15;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->watch_id) {
		g_bus_unwatch_name(mmstate->watch_id);
		mmstate->watch_id = 0;
		agh_log_mm_crit("no longer watching for MM");
	}

	if (mmstate->dbus_connection) {
		g_object_unref(mmstate->dbus_connection);
		mmstate->dbus_connection = NULL;
		agh_log_mm_crit("D-Bus connection object unreferenced");
	}

	mstate->mainloop_needed--;

out:
	return retval;
}

static gint agh_mm_watch_init(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("N AGH or MM context found");
		retval = 12;
		goto out;
	}

	mmstate = mstate->mmstate;

	mmstate->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &mmstate->current_gerror);
	if (!mmstate->dbus_connection) {
		agh_modem_report_gerror_message(&mmstate->current_gerror);
		retval = 13;
		goto out;
	}

	mstate->mainloop_needed++;

	mmstate->watch_id = g_bus_watch_name_on_connection(mmstate->dbus_connection, AGH_MM_ModemManager_DBUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, agh_mm_start, agh_mm_stop, mstate, NULL);
	if (!mmstate->watch_id) {
		agh_log_mm_crit("failure while starting to watch for ModemManager");
		retval = 14;
		goto out;
	}

out:
	if (retval)
		agh_mm_watch_deinit(mstate);

	return retval;
}

gint agh_mm_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH ( / mm ) context found");
		ret = 1;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->mctx) {
		uci_unload(mmstate->mctx, mmstate->uci_package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->uci_package = NULL;
	}

	if (mmstate->watch_id)
		agh_mm_watch_deinit(mstate);
	g_free(mmstate);
	mstate->mmstate = NULL;

out:
	return ret;
}

gint agh_mm_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;
	mmstate = NULL;

	if (!mstate || mstate->mmstate) {
		agh_log_mm_crit("no AGH state or MM context already present");
		ret = -11;
		goto out;
	}

	mmstate = g_try_malloc0(sizeof(*mmstate));
	if (!mmstate) {
		agh_log_mm_crit("can not allocate AGH MM state structure");
		ret = -10;
		goto out;
	}

	mstate->mmstate = mmstate;

	ret = agh_modem_validate_config(mmstate, "agh_modem");
	if (ret) {
		agh_modem_report_gerror_message(&mmstate->current_gerror);
		goto out;
	}

	ret = agh_mm_watch_init(mstate);
	if (ret) {
		agh_modem_report_gerror_message(&mmstate->current_gerror);
		goto out;
	}

out:

	if (ret)
		agh_mm_deinit(mstate);

	return ret;
}

gint agh_modem_report_gerror_message(GError **error) {
	gint retval;

	retval = 0;

	if (!error || !*error) {
		agh_log_mm_crit("asked to report a NULL GError, or one pointing to a NULL pointer");
		retval = 1;
		goto out;
	}

	agh_log_mm_crit("(GError report) - %s",(*error)->message ? (*error)->message : "(no error message)");
	g_error_free(*error);
	*error = NULL;

out:
	return retval;
}

void agh_mm_testwait(gint secs) {
	agh_log_mm_crit("plug/unplug the modem, or do what you feel like");
	g_usleep(secs*G_USEC_PER_SEC);
	return;
}
