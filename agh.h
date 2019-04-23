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

	/* Used to signal we are going to exit. */
	gint exiting;

	/* Idle source for exiting. */
	GSource *exitsrc;
	guint exitsrc_tag;
	GQueue *agh_handlers;

	/* current event ID */
	gint event_id;

	/* comm */
	struct agh_comm *comm;

	/* used by MM to wait for ubus, may be extended to be more general */
	GSource *ubus_wait_src;
	guint ubus_wait_src_tag;
};

/* Function prototypes */
void agh_copy_textparts(gpointer data, gpointer user_data);

struct agh_text_payload {
	gchar *text;
	gchar *source_id;
};

#endif
