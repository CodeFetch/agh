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

void modem_thread_init(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate;

	mmstate = NULL;

	mmstate = g_malloc0(sizeof(struct modem_state));

	ct->thread_data = mmstate;

	ct->handlers = handlers_setup();

	/* Register some handlers here */

	aghservices_messaging_setup(ct, TRUE);

	handlers_init(ct->handlers, ct);

	return;
}

gpointer modem_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate = ct->thread_data;
	struct command *event;

	event = NULL;

	/* We need a connection to D-Bus to talk with ModemManager, of course. */
	mmstate->dbus_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &mmstate->gerror);

	if (!mmstate->dbus_connection) {
		g_print("%s: unable to connect to the D-Bus system bus; error was %s\n",ct->thread_name, mmstate->gerror ? mmstate->gerror->message : "unknown error");
		mm_freebase(mmstate);
		return data;
	}

	/*
	 * Asynchronously obtain a manager object, used to talk to ModemManager. This means we're "requesting" here the object, but the setup will end in another function.
	*/
	g_print("%s: requesting manager object\n",ct->thread_name);
	mm_manager_new(mmstate->dbus_connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)modem_manager_init, ct);
	event = cmd_event_prepare();
	cmd_answer_addtext(event, "test_event");
	cmd_answer_addtext(event, "test_arg1");
	cmd_answer_addtext(event, "test_arg2");
	cmd_answer_addtext(event, "test_arg3");
	cmd_answer_addtext(event, "test_arg4");
	cmd_emit_event(ct->agh_comm, event);
	g_print("%s: entering main loop.\n",ct->thread_name);

	g_main_loop_run(ct->evl);

	return data;
}

void modem_thread_deinit(gpointer data) {
	g_print("Modem thread deinit function invoked.\n");
	return;
}

void mm_freebase(struct modem_state *mmstate) {

	if (mmstate->gerror) {
		g_error_free(mmstate->gerror);
		mmstate->gerror = NULL;
	}

	return;
}

void modem_manager_init(GDBusConnection *connection, GAsyncResult *res, struct agh_thread *ct) {
	struct modem_state *mmstate = ct->thread_data;

	mmstate->manager = mm_manager_new_finish(res, &mmstate->gerror);

	if (!mmstate->manager) {
		g_print("%s: can not obtain a manager object; %s\n",ct->thread_name, mmstate->gerror ? mmstate->gerror->message : "unknown error");
		mm_freebase(mmstate);
		return;
	}

	mmstate->name_owner = g_dbus_object_manager_client_get_name_owner (G_DBUS_OBJECT_MANAGER_CLIENT (mmstate->manager));

	if (!mmstate->name_owner) {
		g_print("%s: can not find the ModemManager process in the bus.\n", ct->thread_name);
		mm_freebase(mmstate);
	}
	return;
}
