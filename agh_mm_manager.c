#include "agh_mm_manager.h"
#include "agh_mm_sm.h"
void agh_mm_manager_init(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data) {
	struct agh_state *mstate = user_data;

	g_print("%s: MM!\n",__FUNCTION__);
	mstate->mainloop_needed++;

	mm_manager_new(connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)agh_mm_manager_init_finish, mstate);

	return;
}

void agh_mm_manager_init_finish(GDBusConnection *connection, GAsyncResult *res, struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	GList *modems;
	GList *l;

	mmstate->manager = mm_manager_new_finish(res, &mmstate->gerror);
	l = NULL;
	modems = NULL;

	if (!mmstate->manager) {
		g_print("%s: can not obtain a manager object; %s\n",__FUNCTION__, mmstate->gerror ? mmstate->gerror->message : "unknown error");
		g_error_free(mmstate->gerror);
		mmstate->gerror = NULL;
		return;
	}

	agh_mm_manager_signals_init(mstate);

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems)
		return;

	for (l = modems; l; l = g_list_next (l)) {
		agh_mm_sm_start(mstate, MM_OBJECT(l->data));
	}
	g_list_free_full(modems, g_object_unref);

	return;
}

void agh_mm_manager_deinit(GDBusConnection *connection, const gchar *name, gpointer user_data) {
	struct agh_state *mstate = user_data;
	struct agh_mm_state *mmstate;

	if (!mstate->mmstate || !mstate->mmstate->manager)
		return;

	mmstate = mstate->mmstate;

	g_print("%s: oops, MM gone away\n",__FUNCTION__);

	agh_mm_manager_signals_deinit(mstate);

	g_object_unref(mmstate->manager);
	mmstate->manager = NULL;

	return;
}

void agh_mm_manager_signals_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;

	mmstate->manager_signal_modem_added_id = g_signal_connect(mmstate->manager, "object-added", G_CALLBACK(agh_mm_sm_device_added), mstate);
	mmstate->manager_signal_modem_removed_id = g_signal_connect(mmstate->manager, "object-removed", G_CALLBACK(agh_mm_sm_device_removed), mstate);

	if ((!mmstate->manager_signal_modem_added_id) || (!mmstate->manager_signal_modem_removed_id)) {
		g_print("%s: can not connect handlers\n",__FUNCTION__);
		return;
	}

	return;
}

void agh_mm_manager_signals_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;

	mmstate = NULL;

	mmstate = mstate->mmstate;

	g_print("%s: disconnecting our signals handlers\n",__FUNCTION__);

	g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_added_id);
	mmstate->manager_signal_modem_added_id = 0;
	g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_removed_id);
	mmstate->manager_signal_modem_removed_id = 0;

	return;
}
