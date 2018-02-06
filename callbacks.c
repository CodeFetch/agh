/* callbacks */
#include "callbacks.h"
#include <glib.h>
#include "messages.h"

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
	g_queue_foreach(mstate->agh_threads, agh_threads_test_sendmsg, mstate);
	return TRUE;
}

void agh_threads_test_sendmsg(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;
	struct agh_message *testm;
	struct test_csp *mycsp;
	static unsigned int testval = 0;

	testm = msg_alloc(sizeof(struct test_csp));
	mycsp = testm->csp;
	mycsp->num = testval;
	testval++;

	g_print("AGH CORE: sending message %d\n", mycsp->num);

	g_async_queue_push(ct->comm, testm);
	return;
}