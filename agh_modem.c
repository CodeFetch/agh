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

static void agh_mm_handle_modem(struct agh_state *mstate, MMObject *modem) {
	return;
}

static void agh_mm_unhandle_modem(struct agh_state *mstate, MMObject *modem) {
	return;
}

static void agh_mm_device_added(MMManager *manager, MMObject *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("modem added");

	agh_mm_handle_modem(mstate, modem);

	return;
}

static void agh_mm_device_removed(MMManager *manager, MMObject *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("modem removed");

	agh_mm_unhandle_modem(mstate, modem);

	return;
}

static gint agh_mm_mngr_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("No AGH state or AGH MM state");
		retval = 20;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->manager) {
		agh_log_mm_crit("disconnecting manager signals");

		if (mmstate->manager_signal_modem_added_id) {
			g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_added_id);
			mmstate->manager_signal_modem_added_id = 0;
		}

		if (mmstate->manager_signal_modem_removed_id) {
			g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_removed_id);
			mmstate->manager_signal_modem_removed_id = 0;
		}

		agh_log_mm_dbg("unreferencing manager object");
		g_object_unref(mmstate->manager);
		mmstate->manager = NULL;
	}

out:
	return retval;
}

static void agh_mm_sm_bootstrap(GDBusConnection *connection, GAsyncResult *res, struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gint error;

	error = 0;

	mmstate->manager = mm_manager_new_finish(res, &mmstate->current_gerror);
	if (!mmstate->manager) {
		agh_modem_report_gerror_message(&mmstate->current_gerror);
		error++;
		goto out;
	}

	mmstate->manager_signal_modem_added_id = g_signal_connect(mmstate->manager, "object-added", G_CALLBACK(agh_mm_device_added), mstate);
	if (!mmstate->manager_signal_modem_added_id) {
		agh_log_mm_crit("can not connect object-added signal");
		error++;
		goto out;
	}

	mmstate->manager_signal_modem_removed_id = g_signal_connect(mmstate->manager, "object-removed", G_CALLBACK(agh_mm_device_removed), mstate);
	if (!mmstate->manager_signal_modem_removed_id) {
		agh_log_mm_crit("can not connect object-removed signal");
		error++;
		goto out;
	}

out:
	if (error)
		agh_mm_mngr_deinit(mstate);

	return;
}

static gint agh_mm_mngr_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gint retval;

	retval = 0;

	if (!mmstate) {
		agh_log_mm_crit("no AGH MM state context");
		retval = 40;
		goto out;
	}

	mm_manager_new(mmstate->dbus_connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)agh_mm_sm_bootstrap, mstate);

out:
	return retval;
}

static void agh_mm_start(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gint retv;

	agh_log_mm_dbg("MM (%s) is now present in the D-Bus system bus (%s)!",name,name_owner);

	retv = agh_mm_mngr_init(mstate);
	if (retv)
		agh_log_mm_crit("manager init will not take place (error code=%" G_GINT16_FORMAT")",retv);

	return;
}

static void agh_mm_stop(GDBusConnection *connection, const gchar *name, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("MM (%s) disappeared from the D-Bus system bus",name);

	agh_mm_mngr_deinit(mstate);

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

	agh_mm_mngr_deinit(mstate);

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

gint agh_mm_report_event(struct agh_state *mstate, const gchar *evname, const gchar *evtext) {
	gint retval;
	struct agh_cmd *ev;

	retval = 0;

	if (!mstate || !evname || !evtext) {
		agh_log_mm_crit("no AGH stare, or NULL event name / event text");
		retval = 25;
		goto out;
	}

	ev = agh_cmd_event_alloc(&retval);
	if (!ev) {
		agh_log_mm_crit("AGH event allocation failure (code=%" G_GINT16_FORMAT")",retval);
		goto out;
	}

	agh_cmd_answer_set_status(ev, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(ev, evname, TRUE);
	agh_cmd_answer_addtext(ev, evtext, TRUE);
	retval = agh_cmd_emit_event(mstate->comm, ev);
	if (retval)
		agh_log_mm_crit("event could not be emitted (code=%" G_GINT16_FORMAT")",retval);

out:
	return retval;
}
