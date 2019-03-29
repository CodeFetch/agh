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
#include "agh_mm_helpers.h"

/* Log messages from AGH_LOG_DOMAIN_MODEM domain. */
#define AGH_LOG_DOMAIN_MODEM "MM"

/* Logging macros. */
#define agh_log_mm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)
#define agh_log_mm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)

static void agh_mm_modem_delete_bearer_finish(MMModem *modem, GAsyncResult *res, gpointer user_data) {
	GError *current_gerror;

	current_gerror = NULL;

	switch(mm_modem_delete_bearer_finish(modem, res, &current_gerror)) {
		case TRUE:
			agh_log_mm_crit("bearer deleted successfully");
			break;
		case FALSE:
			agh_log_mm_crit("can not delete bearer");
			agh_modem_report_gerror_message(&current_gerror);
			break;
	}

	return;
}

static void agh_mm_modem_delete_bearer(gpointer data, gpointer user_data) {
	MMBearer *b = MM_BEARER(data);
	MMModem *modem = MM_MODEM(user_data);
	const gchar *bpath;

	if (!b) {
		agh_log_mm_crit("found a NULL bearer in list!");
		return;
	}

	bpath = mm_bearer_get_path(b);
	agh_log_mm_dbg("requesting for bearer %s to be deleted", bpath);
	mm_modem_delete_bearer(modem, bpath, NULL, (GAsyncReadyCallback)agh_mm_modem_delete_bearer_finish, b);

	return;
}

static void agh_mm_modem_delete_bearers(GObject *o, GAsyncResult *res, gpointer user_data) {
	struct agh_state *mstate = user_data;
	GList *current_bearers;
	GList *l;
	MMModem *modem = MM_MODEM(o);

	current_bearers = mm_modem_list_bearers_finish(modem, res, &mstate->mmstate->current_gerror);
	if (!current_bearers) {
		agh_log_mm_crit("problem when deleting bearers");
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror);
		goto out;
	}

	g_list_foreach(current_bearers, agh_mm_modem_delete_bearer, modem);

	g_list_free_full(current_bearers, g_object_unref);

out:
	return;
}

static gint agh_mm_modem_bearers(struct agh_state *mstate, MMModem *modem, GAsyncReadyCallback cb) {
	gint retval;

	retval = 0;

	if (!modem || !cb) {
		agh_log_mm_crit("NULL modem object, or callback");
		retval = 41;
		goto out;
	}

	agh_log_mm_crit("requesting bearers list");

	mm_modem_list_bearers(modem, NULL, (GAsyncReadyCallback)cb, mstate);

out:
	return retval;
}

static gint agh_mm_modem_signals(struct agh_state *mstate, MMModem *modem, MMModemState state) {
	if (state < MM_MODEM_STATE_REGISTERED)
		agh_log_mm_dbg("may disconnect signals from %s",mm_modem_get_path(modem));
	if (state == MM_MODEM_STATE_REGISTERED)
		agh_log_mm_dbg("may connect signals to %s",mm_modem_get_path(modem));

	return 0;
}

static void agh_mm_modem_enable_finish(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_modem_enable_finish(modem, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_log_mm_dbg("OK");
			break;
		case FALSE:
			agh_log_mm_crit("can not enable modem");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror);
			break;
	}

	return;
}

static gint agh_mm_modem_enable(struct agh_state *mstate, MMModem *modem) {
	gint retval;
	struct uci_section *modem_section;
	struct uci_option *enable_opt;
	gboolean should_enable;

	retval = 0;
	should_enable = TRUE;

	modem_section = agh_mm_config_get_modem_section(mstate, modem);
	if (!modem_section) {
		agh_log_mm_crit("unable to to find a valid configuration section for this modem");
	}
	else {

		enable_opt = uci_lookup_option(mstate->mmstate->mctx, modem_section, AGH_MM_SECTION_MODEM_OPTION_ENABLE);

		switch(agh_mm_config_get_boolean(enable_opt)) {
			case 0:
				agh_log_mm_dbg("modem %s will not be enabled because of configuration",mm_modem_get_path(modem));
				should_enable = FALSE;
				break;
			case 1:
				agh_log_mm_crit("will try to enable modem");
				break;
			default:
				agh_log_mm_crit("invalid option");
				retval = -40;
		}
	}

	if (should_enable)
		mm_modem_enable(modem, NULL, (GAsyncReadyCallback)agh_mm_modem_enable_finish, mstate);

	return retval;
}

static void agh_mm_sim_pin_unlock_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_send_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_log_mm_dbg("unlock was successful! See you!");
			break;
		case FALSE:
			agh_log_mm_crit("unlock failed!");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror);
			break;
	}

	return;
}

static void agh_mm_sim_pin_unlock_stage1(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	MMSim *sim;
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct uci_section *sim_section;
	struct uci_option *pin_option;
	guint left_pin_retries;
	MMUnlockRetries *retries;

	retries = NULL;

	sim = mm_modem_get_sim_finish(modem, res, &mmstate->current_gerror);
	if (!sim) {
		agh_log_mm_crit("unable to get SIM for modem %s",mm_modem_get_path(modem));
		agh_modem_report_gerror_message(&mmstate->current_gerror);
		goto out;
	}

	sim_section = agh_mm_config_get_sim_section(mstate, modem, sim);
	if (!sim_section) {
		agh_log_mm_crit("no configuration data found for this SIM card, can not unlock");
		goto out;
	}

	retries = mm_modem_get_unlock_retries(modem);
	if (!retries) {
		agh_log_mm_crit("unable to get unlock retries for this modem");
		goto out;
	}

	left_pin_retries = mm_unlock_retries_get(retries, MM_MODEM_LOCK_SIM_PIN);
	if (left_pin_retries == MM_UNLOCK_RETRIES_UNKNOWN) {
		agh_log_mm_crit("unable to retrieve retries left (we got MM_UNLOCK_RETRIES_UNKNOWN from mm_unlock_retries_get), %" G_GUINT16_FORMAT"", left_pin_retries);
		goto out;
	}

	if (left_pin_retries <= 2) {
		agh_log_mm_crit("insufficient number of retries left (%" G_GUINT16_FORMAT"); consider manual intervention",left_pin_retries);
		goto out;
	}

	pin_option = uci_lookup_option(mstate->mmstate->mctx, sim_section, AGH_MM_SECTION_SIMCARD_OPTION_PIN_CODE);
	if (!pin_option) {
		agh_log_mm_crit("this can not happen, but... we could not get the PIN-related UCI option");
		goto out;
	}

	if (pin_option->type != UCI_TYPE_STRING) {
		agh_log_mm_crit("the PIN code should be an UCI string");
		goto out;
	}

	agh_log_mm_crit("attempting to unlock modem via SIM PIN");
	mm_sim_send_pin(sim, pin_option->v.string, NULL, (GAsyncReadyCallback)agh_mm_sim_pin_unlock_finish, mstate);

out:
	if (sim)
		g_object_unref(sim);

	if (retries)
		g_object_unref(retries);

	return;
}

static gint agh_mm_sim_pin_unlock(struct agh_state *mstate, MMModem *modem) {
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx || !modem) {
		agh_log_mm_crit("missing needed context");
		retval = 22;
		goto out;
	}

	mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_sim_pin_unlock_stage1, mstate);

out:
	return retval;
}

static gint agh_mm_modem_unlock(struct agh_state *mstate, MMModem *modem) {
	gint retval;
	MMModemLock lock;

	retval = 0;
	lock = mm_modem_get_unlock_required(modem);

	agh_mm_report_event(mstate, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_lock_get_string(lock));
	agh_log_mm_crit("modem %s is locked (%s)",mm_modem_get_path(modem), mm_modem_lock_get_string(lock));

	switch(lock) {
		case MM_MODEM_LOCK_NONE:
			agh_log_mm_crit("oops, we where called on an unlocked modem");
			retval = 19;
			break;
		case MM_MODEM_LOCK_UNKNOWN:
		case MM_MODEM_LOCK_SIM_PIN2:
		case MM_MODEM_LOCK_SIM_PUK:
		case MM_MODEM_LOCK_SIM_PUK2:
		case MM_MODEM_LOCK_PH_SP_PIN:
		case MM_MODEM_LOCK_PH_SP_PUK:
		case MM_MODEM_LOCK_PH_NET_PIN:
		case MM_MODEM_LOCK_PH_NET_PUK:
		case MM_MODEM_LOCK_PH_SIM_PIN:
		case MM_MODEM_LOCK_PH_CORP_PIN:
		case MM_MODEM_LOCK_PH_CORP_PUK:
		case MM_MODEM_LOCK_PH_FSIM_PIN:
		case MM_MODEM_LOCK_PH_FSIM_PUK:
		case MM_MODEM_LOCK_PH_NETSUB_PIN:
		case MM_MODEM_LOCK_PH_NETSUB_PUK:
			agh_log_mm_crit("do not know how to handle this lock");
			retval = 20;
			break;
		case MM_MODEM_LOCK_SIM_PIN:
			retval = agh_mm_sim_pin_unlock(mstate, modem);
			break;
	}

	return retval;
}

static void agh_mm_statechange(MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gint retval;

	retval = 0;

	agh_mm_report_event(mstate, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_get_string(oldstate));
	agh_mm_report_event(mstate, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_get_string(newstate));
	agh_mm_report_event(mstate, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), agh_mm_get_statechange_reason_string(reason));

	retval = agh_mm_modem_signals(mstate, modem, newstate);
	if (retval)
		agh_log_mm_crit("failure from agh_mm_modem_signals (code=%" G_GINT16_FORMAT")",retval);

	switch(newstate) {
		case MM_MODEM_STATE_FAILED:
			agh_mm_report_event(mstate, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem)));
			agh_log_mm_crit("modem %s failed (%s)",mm_modem_get_path(modem),mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem)));
			break;
		case MM_MODEM_STATE_UNKNOWN:
			agh_log_mm_crit("modem %s is in an unknown state, no action is being taken",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_INITIALIZING:
			agh_log_mm_dbg("modem %s is initializing, please wait...",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_LOCKED:
			agh_log_mm_crit("modem %s is currently locked, will try to unlock it",mm_modem_get_path(modem));
			retval = agh_mm_modem_unlock(mstate, modem);

			if (retval)
				agh_log_mm_crit("failure from agh_mm_modem_unlock (code=%" G_GINT16_FORMAT")",retval);

			break;
		case MM_MODEM_STATE_DISABLED:
			retval = agh_mm_modem_bearers(mstate, modem, agh_mm_modem_delete_bearers);
			if (retval)
				agh_log_mm_crit("failure while deleting bearers (code=%" G_GINT16_FORMAT")",retval);

			retval = agh_mm_modem_enable(mstate, modem);
			if (retval)
				agh_log_mm_crit("failure from agh_mm_modem_enable (code=%" G_GINT16_FORMAT")",retval);
			break;
		case MM_MODEM_STATE_DISABLING:
			agh_log_mm_crit("modem %s is being disabled",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_ENABLING:
			agh_log_mm_crit("modem %s is being enabled",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_ENABLED:
			agh_log_mm_crit("modem %s is enabled, waiting for network registration to happen",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_REGISTERED:
			agh_log_mm_crit("modem %s is registered to network!",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_SEARCHING:
			agh_log_mm_crit("modem %s is searching...",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_DISCONNECTING:
			agh_log_mm_crit("modem %s is disconnecting from network",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_CONNECTING:
			agh_log_mm_crit("modem %s is connecting to data network",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_CONNECTED:
			agh_log_mm_crit("modem %s is connected!",mm_modem_get_path(modem));
			break;
	}

	return;
}

static gint agh_mm_handle_modem(struct agh_state *mstate, MMObject *modem) {
	MMModem *m;
	gint retval;
	gulong signal_id;

	retval = 0;

	m = mm_object_get_modem(modem);
	if (!m) {
		agh_log_mm_crit("MMObject not implementing MMModem interface");
		retval = 1;
		goto out;
	}

	signal_id = g_signal_connect(m, "state-changed", G_CALLBACK(agh_mm_statechange), mstate);
	if (!signal_id) {
		agh_log_mm_crit("unable to connect state-changed signal");
		retval = 2;
		goto out;
	}

	agh_mm_statechange(m, MM_MODEM_STATE_UNKNOWN, mm_modem_get_state(m), MM_MODEM_STATE_CHANGE_REASON_UNKNOWN, mstate);

out:
	if (m)
		g_object_unref(m);

	return retval;
}

static gint agh_mm_unhandle_modem(struct agh_state *mstate, MMObject *modem) {
	gint retval;
	gint num_handlers;
	MMModem *m;

	retval = 0;

	m = mm_object_get_modem(modem);
	if (!m) {
		agh_log_mm_crit("can not get modem object durning unhandling");
		retval = 14;
		goto out;
	}

	num_handlers = g_signal_handlers_disconnect_by_func(m, agh_mm_statechange, mstate);
	if (!num_handlers) {
		agh_log_mm_crit("no handlers matched during signal disconnect");
	}
	else
		agh_log_mm_crit("%" G_GINT16_FORMAT" handlers matched during signal disconnect",num_handlers);

out:
	if (m)
		g_object_unref(m);
	return retval;
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

static gint agh_mm_handle_present_modems(struct agh_state *mstate) {
	GList *l;
	GList *modems;
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager) {
		agh_log_mm_crit("AGH state, AGH MM state or manager object where not present");
		retval = 20;
		goto out;
	}

	mmstate = mstate->mmstate;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems) {
		agh_log_mm_dbg("seems no modems have been found (yet?)");
		goto out;
	}

	for (l = modems; l; l = g_list_next(l)) {
		retval = agh_mm_handle_modem(mstate, (MMObject *)(l->data));
		if (retval) {
			agh_log_mm_crit("got failure from agh_mm_handle_modem (code=%" G_GINT16_FORMAT")",retval);
		}
	}

	retval = 0;

	g_list_free_full(modems, g_object_unref);

out:
	return retval;
}

static void agh_mm_bootstrap(GDBusConnection *connection, GAsyncResult *res, struct agh_state *mstate) {
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

	error = agh_mm_handle_present_modems(mstate);
	if (error) {
		agh_log_mm_crit("got failure from agh_mm_handle_present_modems (code=%" G_GINT16_FORMAT")",error);
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

	mm_manager_new(mmstate->dbus_connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)agh_mm_bootstrap, mstate);

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

gint agh_mm_report_event(struct agh_state *mstate, const gchar *evname, gchar *evpath, const gchar *evtext) {
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
		retval = 26;
		goto out;
	}

	agh_cmd_answer_set_status(ev, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(ev, evname, TRUE);

	if (evpath)
		agh_cmd_answer_addtext(ev, evpath, FALSE);

	agh_cmd_answer_addtext(ev, evtext, TRUE);

	retval = agh_cmd_emit_event(mstate->comm, ev);

out:
	if (retval) {
		agh_log_mm_crit("event could not be emitted (code=%" G_GINT16_FORMAT")",retval);
		g_free(evpath);
	}
	return retval;
}
