#ifndef __agh_h__
#define __agh_h__
#include <glib.h>

/* Exit statuses */
#define OUT_OF_MEMORY 2

/* Data structures */
struct agh_state {
	GMainLoop *agh_mainloop;

	/* UNIX Signals stuff */
	GSource *agh_main_unix_signals;
	guint agh_main_unix_signals_tag;
	gboolean sigint_received;

	/* tick */
	GSource *agh_timeout_tick;
	guint agh_timeout_tick_tag;

	/* queue of threads */
	GQueue *agh_threads;
};

struct agh_thread {
	/* core data */
	GThread *current_thread;
	GAsyncQueue *comm_asyncqueue;
	char *thread_name;

	/* callbacks */
	void (*agh_thread_init)(gpointer data);
	gpointer (*agh_thread_main)(gpointer data);
	void (*agh_thread_deinit)(gpointer data);

	/* thread data */
	void *thread_data;
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
void agh_threads_destroied_check(gpointer data);
#endif
