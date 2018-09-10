#ifndef __agh_h__
#define __agh_h__
#include <glib.h>
#include "agh_handlers.h"

/* command used to "quit" AGH */
#define AGH_CMD_QUIT "quit"

#define AGH_RELEASE_NAME "Gato"

#define AGH_VERSION "0.01"

/* Data structures */
struct agh_state {
	GMainLoop *agh_mainloop;
	GMainContext *ctx;

	/* UNIX Signals stuff */
	GSource *agh_main_unix_signals;
	guint agh_main_unix_signals_tag;
	gboolean sigint_received;

	/* ubus */
	struct agh_ubus_ctx *uctx;

	/* XMPP */
	struct xmpp_state *xstate;

	/* Modem */
	struct agh_mm_state *mmstate;

	/* For parts of the program, like XMPP, who need the main loop to be running to properly deinitit. */
	guint mainloop_needed;

	/* Used to  signal we are going to exit. */
	gint exiting;

	/* Idle source for exiting. */
	GSource *exitsrc;
	guint exitsrc_tag;
	GSource *hacksrc;
	guint hacksrc_tag;

	/* our threads and handlers */
	GQueue *agh_threads;
	GQueue *agh_handlers;

	/* current event ID */
	gint event_id;

	/* comm */
	struct agh_comm *comm;
};

struct agh_thread {
	/* glib thread data */
	GThread *current_thread;

	/* used to help users distinguish threads; maybe we can avoid using this ? */
	gchar *thread_name;

	/* Expose AGH main loop and context: useful for threads to cause the core to exit; arguably a good choice. */
	GMainContext *agh_maincontext;
	GMainLoop *agh_mainloop;

	GMainLoop *evl;
	GMainContext *evl_ctx;
	GQueue *handlers;

	/* thread data */
	gpointer thread_data;

	/* thread comm */
	struct agh_comm *comm;

	/* core agh comm ptr */
	struct agh_comm *agh_comm;

	/* callbacks */
	void (*agh_thread_init)(gpointer data);
	gpointer (*agh_thread_main)(gpointer data);
	void (*agh_thread_deinit)(gpointer data);
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

gboolean agh_unix_signals_cb(gpointer data);
gboolean exitsrc_idle_cb(gpointer data);

/* Core command handler */
gpointer core_recvtextcommand_handle(gpointer data, gpointer hmessage);
gpointer core_sendtext_handle(gpointer data, gpointer hmessage);
gpointer core_cmd_handle(gpointer data, gpointer hmessage);
gpointer core_event_to_text_handle(gpointer data, gpointer hmessage);

void agh_core_handlers_setup_ext(struct agh_state *mstate);
void agh_thread_eventloop_setup(struct agh_thread *ct, gboolean as_default_context);
void agh_thread_eventloop_teardown(struct agh_thread *ct);
gpointer agh_thread_default_exit_handle(gpointer data, gpointer hmessage);
void agh_broadcast_exit(struct agh_state *mstate);
void agh_exit(struct agh_state *mstate);
void agh_start_exit(struct agh_state *mstate);
void agh_copy_textparts(gpointer data, gpointer user_data);
gpointer xmppmsg_to_text_handle(gpointer data, gpointer hmessage);

struct text_csp {
	gchar *text;
	gchar *source_id;
};

gboolean hacksrc_idle_cb(gpointer data);

#endif
