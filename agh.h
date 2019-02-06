#ifndef __agh_h__
#define __agh_h__
#include <glib.h>
#include "agh_handlers.h"

/* command used to "quit" AGH */
#define AGH_CMD_QUIT "quit"

/* and for development related testing */
#define AGH_CMD_DEVTEST "test"

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
void agh_copy_textparts(gpointer data, gpointer user_data);
gpointer xmppmsg_to_text_handle(gpointer data, gpointer hmessage);

struct text_csp {
	gchar *text;
	gchar *source_id;
};

#endif
