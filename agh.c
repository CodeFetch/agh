#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <glib.h>
#include <glib-unix.h>
#include "agh.h"
#include "xmpp.h"
#include "callbacks.h"
#include "aghservices.h"
// XXX test only
#include "xmpp_handlers.h"

int main(void) {

	struct agh_state *mstate;

	mstate = agh_state_setup();

	mstate->agh_handlers = handlers_setup();

	// XXX register some handlers here, test only
	handler_register(mstate->agh_handlers, &xmpp_test_handler);

	agh_sources_setup(mstate);

	agh_threads_setup(mstate);

	agh_thread_register(mstate, &xmpp_thread_ops);

	agh_threads_prepare(mstate);

	agh_threads_start(mstate);

	g_queue_foreach(mstate->agh_threads, agh_threads_test_sendmsg, mstate);

	g_print("AGH CORE: Entering main loop...\n");

	g_main_loop_run(mstate->agh_mainloop);

	process_signals(mstate);

	agh_threads_stop(mstate);
	agh_threads_deinit(mstate);
	agh_threads_teardown(mstate);

	g_print("AGH CORE: Main loop exiting.\n");

	agh_sources_teardown(mstate);
	agh_state_teardown(mstate);
}

struct agh_state * agh_state_setup(void) {
	struct agh_state *mstate;

	/* Note: glib will terminate the application should memory allocation fail. */
	mstate = g_malloc0(sizeof *mstate);

	/* Set up the main event loop */
	mstate->ctx = g_main_context_new();
	mstate->agh_mainloop = g_main_loop_new(mstate->ctx, FALSE);

	return mstate;
}

void agh_sources_setup(struct agh_state *mstate) {
	/* Intercepts UNIX signals. This is useful at least to exit the main loop gracefully. SIGINT is also delivered on ctrl+c event. */
	mstate->agh_main_unix_signals = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(mstate->agh_main_unix_signals, agh_unix_signals_cb_dispatch, mstate, NULL);
	mstate->agh_main_unix_signals_tag = g_source_attach(mstate->agh_main_unix_signals, mstate->ctx);

	/* queue for communicating with other threads. */
	mstate->agh_comm = g_async_queue_new();

	/* Communications with other threads */
	aghservices_core_messaging_setup(mstate);
	handlers_init(mstate->agh_handlers);
	return;
}

void agh_sources_teardown(struct agh_state *mstate) {
	/* UNIX signal source */
	g_source_destroy(mstate->agh_main_unix_signals);
	mstate->agh_main_unix_signals_tag = 0;
	g_print("AGH CORE: SIGINT will not be handled from now on.\n");
	// XXX remember to stop messaging!
}

void agh_state_teardown(struct agh_state *mstate) {
	g_main_loop_unref(mstate->agh_mainloop);
	g_main_context_unref(mstate->ctx);
	g_free(mstate);
}

void process_signals(struct agh_state *mstate) {
	if (mstate->sigint_received)
		g_print("\nSIGINT!\n");
}

void agh_threads_start(struct agh_state *mstate) {
	g_print("AGH CORE: starting threads: \n");
	g_queue_foreach(mstate->agh_threads, agh_threads_start_single, mstate);
	g_print("done\n");
}

void agh_thread_register(struct agh_state *mstate, struct agh_thread *ct) {
	if (!ct) {
		g_print("AGH CORE: ** INVALID THREAD REGISTRATION CALL: NULL VALUE NOT ACCEPTABLE AS A THREAD.\n");
		return;
	}
	
	g_print("AGH CORE: registering %s thread: ",ct->thread_name);
	g_queue_push_head(mstate->agh_threads, ct);
	g_print(" done\n");
}

void agh_threads_stop(struct agh_state *mstate) {
	g_print("Stopping threads: ");

	g_queue_foreach(mstate->agh_threads, agh_threads_stop_single, mstate);
	g_print("done\n");
}

void agh_threads_setup(struct agh_state *mstate) {
	g_print("AGH CORE: Allocating threads queue: ");
	mstate->agh_threads = g_queue_new();
	g_print("done\n");
}

void agh_threads_teardown(struct agh_state *mstate) {
	g_print("AGH CORE: deallocating threads queue.\n");
	g_queue_free(mstate->agh_threads);
}

void agh_threads_prepare(struct agh_state *mstate) {
	g_print("AGH CORE: preparing threads \n");

	g_queue_foreach(mstate->agh_threads, agh_threads_prepare_single, mstate);
	g_print("done\n");
}

void agh_threads_deinit(struct agh_state *mstate) {
	g_print("AGH CORE: invoking threads deinit functions ");
	g_queue_foreach(mstate->agh_threads, agh_threads_deinit_single, mstate);
	g_print("done\n");
}

void agh_threads_prepare_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print(ct->thread_name);

	/* Before starting our thread, provide basic facilities: the communication asynchronous queue and the main thread's GMainContext. */
	ct->agh_maincontext = mstate->ctx;
	ct->agh_mainloop = mstate->agh_mainloop;
	ct->comm = g_async_queue_new();
	ct->handlers = NULL;
	ct->agh_thread_init(ct);
}

void agh_threads_deinit_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print(ct->thread_name);
	ct->agh_thread_deinit(ct);

	if (ct->handlers) {
		g_print("WARNING: %s thread may not be deinitializing its handlers correctly. This needs to be investigated.\n", ct->thread_name);
	}
	g_async_queue_unref(ct->comm);
	ct->agh_maincontext = NULL;
	ct->agh_mainloop = NULL;

	/* don't get confused, this was only to remind us to check how things go when stopping the usage of on-stack control thread structures */
	if (!ct->on_stack ) {
		g_print("** WARNING: AGH thread control structure for %s isn't on stack and has not been freed. Freeing it now, but this needs to be looked at.\n",ct->thread_name);
		g_free(ct);
	}

	/* XXX: a better way to do this? */
	g_queue_remove(mstate->agh_threads, ct);
	return;
}

void agh_threads_start_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print(ct->thread_name);
	ct->current_thread = g_thread_new(ct->thread_name, ct->agh_thread_main, ct);
	return;
}

void agh_threads_stop_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print(ct->thread_name);
	ct->current_thread = g_thread_join(ct->current_thread);
}

