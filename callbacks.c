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

GQueue *handlers_setup(void) {
	return g_queue_new();
}

void handlers_teardown(GQueue *handlers) {
	guint num_handlers;

	num_handlers = g_queue_get_length(handlers);
	if (num_handlers) {
		g_print("handlers: %d handlers are still registered, this is going to leak memory!\n",num_handlers);
	}
	g_queue_free(handlers);
	return;
}

void handler_register(GQueue *handlers, struct handler *h) {
	if ((!h) || (!handlers)) {
		g_print("handlers: tried to register a NULL handler, or to add an handler to a NULL queue.\n");
	}
	else {
		g_queue_push_head(handlers, h);
	}
	return;
}

void handlers_init(GQueue *handlers, gpointer data) {
	g_queue_foreach(handlers, handlers_init_single, data);
	return;
}

void handlers_init_single(gpointer data, gpointer user_data) {
	struct handler *h = data;
	gpointer handler_data = user_data;

	h->handler_initialize(handler_data);
	return;
}

void handlers_finalize_single(gpointer data, gpointer user_data) {
	struct handler *h = data;
	gpointer handler_data = user_data;

	h->handler_finalize(handler_data);
	return;
}

/*
 * This function is not meant to be caleld from inside other handlers related functions.
*/
void handler_unregister(GQueue *handlers, struct handler *h, gpointer data) {
	h->handler_finalize(data);
	g_queue_remove(handlers, h);
	return;
}

void handlers_finalize(GQueue *handlers, gpointer data) {
	g_queue_foreach(handlers, handlers_finalize_single, data);
}
