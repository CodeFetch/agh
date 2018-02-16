#ifndef __agh_h__
#define __agh_h__
#include <glib.h>

/* Data structures */
struct agh_state {
	GMainLoop *agh_mainloop;
	GMainContext *ctx;

	/* UNIX Signals stuff */
	GSource *agh_main_unix_signals;
	guint agh_main_unix_signals_tag;
	gboolean sigint_received;

	/* queue of threads */
	GQueue *agh_threads;

	/* handlers */
	GQueue *agh_handlers;

	/* even core needs to receive messages */
	GAsyncQueue *agh_comm;
	GSource *comm_timeout;
	guint comm_timeout_tag;
};

struct agh_thread {
	/* glib thread data */
	GThread *current_thread;

	/* used to help users distinguish threads; maybe we can avoid using this ? */
	char *thread_name;

	/* Expose AGH main loop and context: useful for threads to cause the core to exit; arguably a good choice. */
	GMainContext *agh_maincontext;
	GMainLoop *agh_mainloop;

	/*
	 * Communication related data; here we include also the GMainLoop related stuff, because we actually need that. or this is the case now.
	*/
	GMainLoop *evl;
	GMainContext *evl_ctx;
	GAsyncQueue *comm;
	GSource *comm_timeout;
	guint comm_timeout_tag;

	/* to be removed at some point */
	gboolean on_stack;

	/* callbacks */
	void (*agh_thread_init)(gpointer data);
	gpointer (*agh_thread_main)(gpointer data);
	void (*agh_thread_deinit)(gpointer data);

	/* thread data */
	gpointer thread_data;
	GQueue *handlers;
};

/* Function prototypes */

/* State handling */
struct agh_state * agh_state_setup(void);
void agh_state_teardown(struct agh_state *mstate);

/* Sources */
void agh_sources_setup(struct agh_state *mstate);
void agh_sources_teardown(struct agh_state *mstate);

/* Signals */
void process_signals(struct agh_state *mstate);

/* Threads */
void agh_threads_setup(struct agh_state *mstate);
void agh_thread_register(struct agh_state *mstate, struct agh_thread *ct);
void agh_threads_prepare(struct agh_state *mstate);
void agh_threads_prepare_single(gpointer data, gpointer user_data);
void agh_threads_start(struct agh_state *mstate);
void agh_threads_start_single(gpointer data, gpointer user_data);
void agh_threads_stop(struct agh_state *mstate);
void agh_threads_stop_single(gpointer data, gpointer user_data);
void agh_threads_deinit(struct agh_state *mstate);
void agh_threads_deinit_single(gpointer data, gpointer user_data);
void agh_threads_teardown(struct agh_state *mstate);
#endif
