/*
 * So here we are, for the second time: after the HDD breakage.
 *
 * Most of the initialization tasks are going to be performed in the thread startup function, not the init one. this is because
 * it seems better to build as much objects as possible in the thread that will end up using them.
*/

#include <glib.h>
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

	return;
}

gpointer modem_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct modem_state *mmstate = ct->thread_data;
	struct command *event;
	gint i = 0;

	event = cmd_event_prepare();
	cmd_answer_addtext(event, "modem_thread_started");
	cmd_emit_event(ct->agh_comm, event);

	return data;
}

void modem_thread_deinit(gpointer data) {
	g_print("Modem thread deinit function invoked.\n");
	return;
}
