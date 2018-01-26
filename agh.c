#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <glib.h>
#include <glib-unix.h>
#include "agh.h"
#include "xmpp.h"
#include "callbacks.h"

int main(void) {

	/* AGH state */
	struct agh_state mstate;

	explicit_bzero(&mstate, sizeof(mstate));
	agh_state_setup(&mstate);

	agh_start_threads(&mstate);

	g_print("Entering main loop...\n");

	g_main_loop_run(mstate.agh_mainloop);

	process_signals(&mstate);
	agh_stop_threads(&mstate);

	g_print("Main loop exiting.\n");

	agh_state_teardown(&mstate);

}

void agh_state_setup(struct agh_state *mstate) {
	/* Set up the main event loop */
	mstate->agh_mainloop = g_main_loop_new(NULL, FALSE);

	/* Intercepts UNIX signals. This is useful at least to exit the main loop gracefully. SIGINT is also delivered on ctrl+c event. */
	mstate->agh_main_unix_signals = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(mstate->agh_main_unix_signals, agh_unix_signals_cb_dispatch, mstate, NULL);
	mstate->agh_main_unix_signals_tag = g_source_attach(mstate->agh_main_unix_signals, NULL);

	/* Emit some "ticks" on the screen: just to know what's happening. */
	mstate->agh_timeout_tick = g_timeout_source_new_seconds(2);
	g_source_set_callback(mstate->agh_timeout_tick, agh_timeout_cb_dispatch, mstate, NULL);
	mstate->agh_timeout_tick_tag = g_source_attach(mstate->agh_timeout_tick, NULL);

	/* Glib Async Queue for communications with the XMPP thread */
	mstate->xmpp_queue = g_async_queue_new();
}

void agh_state_teardown(struct agh_state *mstate) {
	/* UNIX signal source */
	g_source_unref(mstate->agh_main_unix_signals);
	mstate->agh_main_unix_signals_tag = 0;

	/* tick timer */
	g_source_unref(mstate->agh_timeout_tick);
	mstate->agh_timeout_tick_tag = 0;

	/* XMPP asynchronous queue */
	g_async_queue_unref(mstate->xmpp_queue);

	/* At last, main loop. */
	g_main_loop_unref(mstate->agh_mainloop);
}

void process_signals(struct agh_state *mstate) {
	if (mstate->sigint_received)
		g_print("\nSIGINT\n");
}

void agh_start_threads(struct agh_state *mstate) {
	mstate->xmpp_thread = g_thread_new("XMPP", xmpp_start, NULL);
}

void agh_stop_threads(struct agh_state *mstate) {
	if (g_async_queue_length(mstate->xmpp_queue)) {
		g_print("**** BAD XMPP THREAD HANDLING DETECTED ****\n");
	}
	g_print("Should stop threads.\n");
}
