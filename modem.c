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
	mmstate->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &mmstate->gerror);

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
		modem_free_asyncstate(mmstate);
		if (mmstate->disabled_modems)
			g_list_free_full(mmstate->disabled_modems, g_object_unref);
		/* fall through */
	case AGH_MM_NO_MM_LIST:
		if (mmstate->name_owner) {
			g_free(mmstate->name_owner);
			mmstate->name_owner = NULL;
		}
		/* fall through */
	case AGH_MM_NO_MM_PROCESS:
		if (mmstate->manager) {
			g_object_unref(mmstate->manager);
			mmstate->manager = NULL;
		}
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

	mmstate->name_owner = g_dbus_object_manager_client_get_name_owner(G_DBUS_OBJECT_MANAGER_CLIENT(mmstate->manager));

	if (!mmstate->name_owner) {
		g_print("%s: can not find the ModemManager process in the bus.\n", ct->thread_name);
		agh_mm_freemem(mmstate, AGH_MM_NO_MM_PROCESS);
		return;
	}

	mmstate->disabled_modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!mmstate->disabled_modems) {
		agh_mm_freemem(mmstate, AGH_MM_NO_MM_LIST);
		return;
	}

	if (!g_list_length(mmstate->disabled_modems)) {
		g_list_free(mmstate->disabled_modems);
		return;
	}

	/* Asynchronously but sequentially enable all modems we can. */
	agh_modem_enable_all(mmstate);

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

void modem_free_asyncstate(struct modem_state *mstate) {

	if (!mstate->astate) {
		g_print("Not deallocating a NULL async state: this is OK only during deinit.\n");
		return;
	}

	g_print("%s: freeing async state\n",__FUNCTION__);

	if (mstate->astate->acmd)
		cmd_free(mstate->astate->acmd);

	if (mstate->astate->mmobject)
		g_object_unref(mstate->astate->mmobject);

	mstate->astate = NULL;
	mstate->async_pending = FALSE;

	return;
}

void modem_new_asyncstate(struct modem_state *mstate) {

	if (mstate->astate) {
		g_print("An sync state is already allocated; not allocating a new one.\n");
		return;
	}

	mstate->astate = g_malloc0(sizeof(struct modem_async_state));
	mstate->async_pending = TRUE;
	g_print("%s: async state allocated\n", __FUNCTION__);

	return;
}

void agh_modem_enable_all(struct modem_state *mmstate) {
	MMModem *m;

	m = NULL;

	/* We know the list is not NULL and it contains at least one element. */

	modem_new_asyncstate(mmstate);

	m = agh_modem_enable_process_list(mmstate);

	if (!m) {
		/* No modem could be enabled, because no call to mm_object_get_modem returned a suitable object. */
		modem_free_asyncstate(mmstate);
		return;
	}

	mm_modem_enable(m, NULL, (GAsyncReadyCallback)agh_modem_enable_next, mmstate);

	return;
}

void agh_modem_enable_next(MMModem *modem, GAsyncResult *res, struct modem_state *mmstate) {
	gboolean modem_enabled;
	MMModem *nm;
	struct modem_async_state *astate = mmstate->astate;

	modem_enabled = FALSE;
	nm = NULL;

	modem_enabled = mm_modem_enable_finish(modem, res, &mmstate->gerror);

	/* We no longer need the MMModem object, so let it go. */
	g_object_unref(modem);

	if (modem_enabled) {
		/* The modem was enabled successfully. So remove it from the list. */
		mmstate->disabled_modems = g_list_remove(mmstate->disabled_modems, astate->mmobject);

		/* And also the current MMObject. */
		g_object_unref(astate->mmobject);
		astate->mmobject = NULL;
	}
	else {
		g_print("Enabling modem failed: %s\n",mmstate->gerror ? mmstate->gerror->message : "unknown error");
		g_error_free(mmstate->gerror);
		mmstate->gerror = NULL;
	}

	nm = agh_modem_enable_process_list(mmstate);

	if (!nm) {
		modem_free_asyncstate(mmstate);
		mmstate->ready = TRUE;
		g_print("Modem: accepting commands.\n");

		if (!g_list_length(mmstate->disabled_modems)) {
			g_print("All modems where enabled.\n");
			g_list_free(mmstate->disabled_modems);
			mmstate->disabled_modems = NULL;
		}

		return;
	}

	mm_modem_enable(nm, NULL, (GAsyncReadyCallback)agh_modem_enable_next, mmstate);

	return;
}

MMModem *agh_modem_enable_process_list(struct modem_state *mmstate) {
	MMModem *m;
	struct modem_async_state *astate = mmstate->astate;

	m = NULL;

	while ( (mmstate->disabled_modems = g_list_first(mmstate->disabled_modems)) ) {
		astate->mmobject = MM_OBJECT(mmstate->disabled_modems->data);
		m = mm_object_get_modem(astate->mmobject);

		if (m)
			break;
		else
			continue;

	}

	if (!m)
		astate->mmobject = NULL;

	return m;
}
