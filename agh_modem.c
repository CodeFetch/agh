/*
 * So here we are, for the second time: after the HDD breakage.
 *
 * Most of the initialization tasks are going to be performed in the thread startup function, not the init one. this is because
 * it seems better to build as much objects as possible in the thread that will end up using them.
*/

#include <glib.h>
#include <libmm-glib.h>
#include "agh_modem.h"
#include "agh.h"
#include "agh_handlers.h"
#include "agh_mm_handlers.h"
#include "agh_mm_manager.h"
#include "agh_modem_config.h"
#include "agh_mm_helpers_sm.h"

void agh_mm_freemem(struct agh_mm_state *mmstate, gint error) {
	switch(error) {
	case AGH_MM_ERROR_DEINIT:
		g_print("%s: AGH_MM_DEINIT\n",__FUNCTION__);
		if (mmstate->name_owner) {
			g_free(mmstate->name_owner);
			mmstate->name_owner = NULL;
		}
		/* fall through */
	case AGH_MM_ERROR_NO_MM_PROCESS:
		if (mmstate->manager) {
			g_object_unref(mmstate->manager);
			mmstate->manager = NULL;
		}
		break;
	case AGH_MM_ERROR_NO_MANAGER_OBJECT:
	case AGH_MM_ERROR_NO_WATCH_ID:
		if (mmstate->dbus_connection) {
			g_object_unref(mmstate->dbus_connection);
			mmstate->dbus_connection = NULL;
		}
		/* fall through */
	case AGH_MM_ERROR_NO_DBUS_CONNECTION:
		if (mmstate->gerror) {
			g_error_free(mmstate->gerror);
			mmstate->gerror = NULL;
		}
		break;
	default:
		g_print("%s detected an unknown error!\n",__FUNCTION__);
	}

	return;
}

void agh_mm_handlers_setup_ext(struct agh_state *mstate) {
	struct handler *agh_mm_cmd_handler;

	agh_mm_cmd_handler = handler_new("agh_mm_cmd_handler");
	handler_set_handle(agh_mm_cmd_handler, agh_mm_cmd_handle);
	handler_enable(agh_mm_cmd_handler, TRUE);

	handler_register(mstate->agh_handlers, agh_mm_cmd_handler);

	return;
}

void agh_mm_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	struct agh_modem_config_validation_error *validation_error;

	validation_error = NULL;

	mmstate = g_try_malloc0(sizeof(struct agh_mm_state));
	if (!mmstate) {
		g_print("%s: can not allocate mmstate\n",__FUNCTION__);
		return;
	}

	mstate->mmstate = mmstate;

	agh_modem_validate_config(mstate, AGH_MODEM_UCI_CONFIG_PACKAGE, &validation_error);

	if (validation_error) {
		g_print("Failure %" G_GINT16_FORMAT" (%s): %s\n",validation_error->error_code,validation_error->element_name ? validation_error->element_name : "**",validation_error->error_desc);
		agh_modem_config_validation_error_free(validation_error);
		validation_error = NULL;
		mmstate->mctx = NULL;
		mmstate->package = NULL;
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	mmstate->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &mmstate->gerror);

	if (!mmstate->dbus_connection) {
		g_print("%s: can not connect to the system bus; %s\n",__FUNCTION__, mmstate->gerror->message ? mmstate->gerror->message : "unknown error");
		agh_mm_freemem(mmstate, AGH_MM_ERROR_NO_DBUS_CONNECTION);
		uci_unload(mmstate->mctx, mmstate->package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->package = NULL;
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	mmstate->watch_id = g_bus_watch_name_on_connection(mmstate->dbus_connection, AGH_MM_MM_DBUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, agh_mm_manager_init, agh_mm_manager_deinit, mstate, NULL);
	if (!mmstate->watch_id) {
		g_print("%s: can no watch bus for names (was trying to watch for "AGH_MM_MM_DBUS_NAME")\n",__FUNCTION__);
		agh_mm_freemem(mmstate, AGH_MM_ERROR_NO_WATCH_ID);
		uci_unload(mmstate->mctx, mmstate->package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->package = NULL;
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	g_print("%s: waiting for MM\n",__FUNCTION__);
	agh_mm_handlers_setup_ext(mstate);

	return;
}

void agh_mm_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;

	mmstate = NULL;

	if (!mstate->mmstate) {
		g_print("%s: manager was not initialized, hence no deinit is taking place\n",__FUNCTION__);
		return;
	}

	mmstate = mstate->mmstate;

	g_bus_unwatch_name(mmstate->watch_id);
	mmstate->watch_id = 0;

	agh_mm_freemem(mmstate, AGH_MM_ERROR_DEINIT);
	agh_mm_freemem(mmstate, AGH_MM_ERROR_NO_WATCH_ID);
	if (mmstate->mctx) {
		uci_unload(mmstate->mctx, mmstate->package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->package = NULL;
	}

	g_free(mmstate);
	mstate->mmstate = NULL;
	mmstate = NULL;

	return;
}

void agh_mm_start_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	GList *modems;
	struct agh_mm_asyncstate *a;
	MMModem *modem;
	MMObject *mm_object;

	if (!mmstate || !mmstate->manager)
		return;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));
	a = NULL;
	modem = NULL;
	mm_object = NULL;

	if (!modems) {
		mstate->mainloop_needed--;
		return;
	}

	g_list_foreach(modems, agh_mm_select_modems, &modems);

	if (modems) {

		a = g_malloc0(sizeof(struct agh_mm_asyncstate));
		a->blist = modems;
		a->mstate = mstate;

		a->blist = g_list_first(a->blist);
		mm_object = MM_OBJECT(a->blist->data);
		a->blist = g_list_remove(a->blist, a->blist->data);
		modem = mm_object_get_modem(mm_object);
		g_object_unref(mm_object);
		mm_modem_disable(modem, NULL, (GAsyncReadyCallback)agh_mm_disable_all_modems, a);
		g_object_unref(modem);

	}
	else {
		mstate->mainloop_needed--;
		return;
	}

	return;
}

void agh_mm_select_modems(gpointer data, gpointer user_data) {
	MMModemState modem_state;
	MMObject *mm_object = MM_OBJECT(data);
	GList **modems = user_data;
	MMModem *modem;

	modem = NULL;

	modem = mm_object_get_modem(mm_object);
	if (!modem) {
			*modems = g_list_remove(*modems, data);
			g_object_unref(mm_object);
	}

	modem_state = mm_modem_get_state(modem);

	switch(modem_state) {
		case MM_MODEM_STATE_INITIALIZING:
		case MM_MODEM_STATE_ENABLING:
		case MM_MODEM_STATE_ENABLED:
		case MM_MODEM_STATE_SEARCHING:
		case MM_MODEM_STATE_REGISTERED:
		case MM_MODEM_STATE_DISCONNECTING:
		case MM_MODEM_STATE_CONNECTING:
		case MM_MODEM_STATE_CONNECTED:
			break;
		default:
			*modems = g_list_remove(*modems, data);
			g_object_unref(mm_object);
	}

	g_object_unref(modem);

	return;
}

void agh_mm_disable_all_modems(MMModem *modem, GAsyncResult *res, gpointer user_data) {
	gboolean op_res;
	struct agh_mm_asyncstate *a = user_data;
	MMObject *mm_object;

	op_res = mm_modem_disable_finish (modem, res, &a->mstate->mmstate->gerror);
	if (!op_res) {
		agh_mm_sm_report_failure_modem(a->mstate, modem, AGH_MM_SM_MODEM_DEINIITSTATE_FAILURE);
	}

	if (a->blist) {
		a->blist = g_list_first(a->blist);
		mm_object = MM_OBJECT(a->blist->data);
		a->blist = g_list_remove(a->blist, a->blist->data);
		modem = mm_object_get_modem(mm_object);
		g_object_unref(mm_object);
		mm_modem_disable(modem, NULL, (GAsyncReadyCallback)agh_mm_disable_all_modems, a);
		g_object_unref(modem);
	}
	else {
		a->mstate->mainloop_needed--;
		g_free(a);
	}

	return;
}
