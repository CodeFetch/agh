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
	handler_enable(agh_mm_cmd_handler, FALSE);

	handler_register(mstate->agh_handlers, agh_mm_cmd_handler);

	return;
}

void agh_modem_modem_properties_changed(GDBusProxy *manager, GVariant *changed_props, GStrv inv_props, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gchar *test;

	test = g_variant_print(changed_props, TRUE);
	g_print("Changes: %s\n",test);
	g_free(test);
	test = NULL;

	return;
}

void agh_mm_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;

	mmstate = g_malloc0(sizeof(struct agh_mm_state));

	mstate->mmstate = mmstate;

	mmstate->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &mmstate->gerror);

	if (!mmstate->dbus_connection) {
		g_print("%s: can not connect to the system bus; %s\n",__FUNCTION__, mmstate->gerror->message ? mmstate->gerror->message : "unknown error");
		agh_mm_freemem(mmstate, AGH_MM_ERROR_NO_DBUS_CONNECTION);
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	mmstate->watch_id = g_bus_watch_name_on_connection(mmstate->dbus_connection, AGH_MM_MM_DBUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, agh_mm_manager_init, agh_mm_manager_deinit, mstate, NULL);
	if (!mmstate->watch_id) {
		g_print("%s: can no watch bus for names (was trying to watch for "AGH_MM_MM_DBUS_NAME")\n",__FUNCTION__);
		agh_mm_freemem(mmstate, AGH_MM_ERROR_NO_WATCH_ID);
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	g_print("%s: waiting for MM\n",__FUNCTION__);

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
	g_free(mmstate);
	mstate->mmstate = NULL;
	mmstate = NULL;

	return;
}
