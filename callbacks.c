/* callbacks */
#include "callbacks.h"
#include <glib.h>

/* Invoked upon SIGINT reception. */
int agh_unix_signals_cb_dispatch(gpointer data) {
	struct agh_state *mstate = data;
	mstate->sigint_received = TRUE;
	g_main_loop_quit(mstate->agh_mainloop);
	return FALSE;
}

int agh_timeout_cb_dispatch(gpointer data) {
	struct agh_state *mstate = data;
	g_print("AGH CORE: TICK\n");
	return TRUE;
}
