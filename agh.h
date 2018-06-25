#ifndef __agh_h__
#define __agh_h__
#include <glib.h>
#include "handlers.h"

/* command used to "quit" AGH */
#define AGH_CMD_QUIT "quit"

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

	/* current event ID */
	gint event_id;
};

struct agh_thread {
	/* glib thread data */
	GThread *current_thread;

	/* used to help users distinguish threads; maybe we can avoid using this ? */
	gchar *thread_name;

	/* Expose AGH main loop and context: useful for threads to cause the core to exit; arguably a good choice. */
	GMainContext *agh_maincontext;
	GMainLoop *agh_mainloop;
	GAsyncQueue *agh_comm;

	GMainLoop *evl;
	GMainContext *evl_ctx;
	GAsyncQueue *comm;
	GSource *comm_timeout;
	guint comm_timeout_tag;

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

/* threads structures helpers */
struct agh_thread *agh_thread_new(gchar *name);
void agh_thread_set_init(struct agh_thread *ct, void (*agh_thread_init_cb)(gpointer data));
void agh_thread_set_main(struct agh_thread *ct, gpointer (*agh_thread_main_cb)(gpointer data));
void agh_thread_set_deinit(struct agh_thread *ct, void (*agh_thread_deinit_cb)(gpointer data));

gboolean agh_unix_signals_cb_dispatch(gpointer data);

/* Core command handler */

gpointer core_recvtextcommand_handle(gpointer data, gpointer hmessage);
gpointer core_sendtext_handle(gpointer data, gpointer hmessage);
gpointer core_cmd_handle(gpointer data, gpointer hmessage);
gpointer core_event_to_text_handle(gpointer data, gpointer hmessage);

void agh_thread_setup_ext(struct agh_state *mstate);
void agh_core_handlers_setup_ext(struct agh_state *mstate);

struct text_csp {
	gchar *text;
};

#endif
