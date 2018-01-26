#ifndef __agh_h__
#define __agh_h__
#include <glib.h>


struct agh_state {
	GMainLoop *agh_mainloop;

	/* UNIX Signals stuff */
	GSource *agh_main_unix_signals;
	guint agh_main_unix_signals_tag;
	gboolean sigint_received;

	/* tick */
	GSource *agh_timeout_tick;
	guint agh_timeout_tick_tag;

	/* XMPP threads, and communication channels with it */
	GThread *xmpp_thread;
	GAsyncQueue *xmpp_queue;
};

void agh_state_setup(struct agh_state *mstate);
void agh_state_teardown(struct agh_state *mstate);
void process_signals(struct agh_state *mstate);
void agh_start_threads(struct agh_state *mstate);
void agh_stop_threads(struct agh_state *mstate);
#endif
