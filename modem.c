/*
 * So here we are, for the second time: after the HDD breakage.
 *
 * Most of the initialization tasks are going to be performed in the thread startup function, not the init one. this is because
 * it seems better to build as much objects as possible in the thread that will end up using them.
*/

#include <glib.h>
#include <libmm-glib.h>
#include "agh.h"
#include "aghservices.h"
#include "handlers.h"
#include "modem.h"
#include "commands.h"
#include "modem_handlers.h"

void modem_thread_init(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate;

	mmstate = NULL;

	mmstate = g_malloc0(sizeof(struct modem_state));

	ct->thread_data = mmstate;

	ct->handlers = handlers_setup();

	modem_handlers_setup_ext(ct);

	aghservices_messaging_setup(ct, TRUE);

	handlers_init(ct->handlers, ct);

	return;
}

gpointer modem_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate = ct->thread_data;

	/* We need a connection to D-Bus to talk with ModemManager, of course. */
	mmstate->dbus_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &mmstate->gerror);

	if (!mmstate->dbus_connection) {
		g_print("%s: unable to connect to the D-Bus system bus; error was %s\n",ct->thread_name, mmstate->gerror ? mmstate->gerror->message : "unknown error");
		agh_mm_freemem(mmstate, AGH_MM_NO_DBUS_CONNECTION);
		return data;
	}

	/*
	 * Asynchronously obtain a manager object, used to talk to ModemManager. This means we're "requesting" here the object, but the setup will happen once the main loop is started, and in the modem_manager_init function.
	*/
	mm_manager_new(mmstate->dbus_connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)modem_manager_init, ct);

	g_main_loop_run(ct->evl);

	return data;
}

void modem_thread_deinit(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate = ct->thread_data;

	if (!mmstate)
		return;

	agh_mm_freemem(mmstate, AGH_MM_DEINIT);
	g_free(mmstate);
	handlers_finalize(ct->handlers);
	handlers_teardown(ct->handlers);
	ct->handlers = NULL;
	return;
}

void agh_mm_freemem(struct modem_state *mmstate, gint error) {
	switch(error) {
	case AGH_MM_DEINIT:
		g_print("%s: AGH_MM_DEINIT\n",__FUNCTION__);
		if (mmstate->name_owner) {
			g_free(mmstate->name_owner);
			mmstate->name_owner = NULL;
		}
		/* fall through */
	case AGH_MM_NO_MM_PROCESS:
		g_object_unref(mmstate->manager);
		mmstate->manager = NULL;
		/* fall through */
	case AGH_MM_NO_MANAGER_OBJECT:
	case AGH_MM_NO_DBUS_CONNECTION:
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

void modem_manager_init(GDBusConnection *connection, GAsyncResult *res, struct agh_thread *ct) {
	struct modem_state *mmstate = ct->thread_data;

	mmstate->manager = mm_manager_new_finish(res, &mmstate->gerror);

	if (!mmstate->manager) {
		g_print("%s: can not obtain a manager object; %s\n",ct->thread_name, mmstate->gerror ? mmstate->gerror->message : "unknown error");
		agh_mm_freemem(mmstate, AGH_MM_NO_MANAGER_OBJECT);
		return;
	}

	mmstate->name_owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (mmstate->manager));

	if (!mmstate->name_owner) {
		g_print("%s: can not find the ModemManager process in the bus.\n", ct->thread_name);
		agh_mm_freemem(mmstate, AGH_MM_NO_MM_PROCESS);
	}

	return;
}

void modem_handlers_setup_ext(struct agh_thread *ct) {
	struct handler *modem_cmd_handler;

	modem_cmd_handler = handler_new("modem_cmd_handler");
	handler_set_handle(modem_cmd_handler, modem_cmd_handle);
	handler_enable(modem_cmd_handler, TRUE);

	handler_register(ct->handlers, modem_cmd_handler);

	return;
}
