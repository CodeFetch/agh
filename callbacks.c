/* callbacks */
#include "callbacks.h"
#include <glib.h>
#include "messages.h"

/* Invoked upon SIGINT reception. */
gboolean agh_unix_signals_cb_dispatch(gpointer data) {
	struct agh_state *mstate = data;
	mstate->sigint_received = TRUE;
	g_main_loop_quit(mstate->agh_mainloop);
	return FALSE;
}

gboolean agh_timeout_cb_dispatch(gpointer data) {
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
	msg_prepare(testm, mstate->agh_comm, ct->comm);
	mycsp = testm->csp;
	mycsp->num = testval;
	testval++;

	g_print("AGH CORE: sending message %d\n", mycsp->num);

	msg_send(testm);
	return;
}

GQueue *handlers_setup(void) {
	g_print("handlers: allocating queue.\n");
	return g_queue_new();
}

void handlers_teardown(GQueue *handlers) {
	g_print("handlers: teardown in progress.\n");
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
		g_print("handlers: an handler has been registered.\n");
	}
	return;
}

void handlers_init(GQueue *handlers) {
	g_print("handlers: init is taking place.\n");
	g_queue_foreach(handlers, handlers_init_single, NULL);
	return;
}

void handlers_init_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	g_print("*");
	if (h->enabled)
		h->handler_initialize(h);
	return;
}

void handlers_finalize_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	g_print("X");
	h->handler_finalize(h);

	/* XXX: a better way to do this? */
	g_queue_remove(h->handlers_queue, h);
	if (!h->on_stack) {
		g_print("handlers: freeing an handler that has not been allocated in, or declared to be, in the stack. Freeing it now, but this needs to be looked at.\n");
		g_free(h);
	}
	return;
}

void handlers_finalize(GQueue *handlers) {
	g_print("handlers: finalizing handlers.\n");
	g_queue_foreach(handlers, handlers_finalize_single, NULL);
	return;
}

struct agh_message *handlers_dispatch_single(gpointer data, gpointer user_data) {
	struct handler *h = data;
	struct agh_message *m = user_data;
	struct agh_message *answer;

	answer = h->handle(data, user_data);

	return answer;
}
