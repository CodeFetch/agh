#include <glib.h>
#include "agh.h"
#include "xmpp.h"

void xmpp_thread_init(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate;

	ct->thread_data = g_malloc0(sizeof(struct xmpp_state));

	/* XXX proper error checking! */
	if (!ct->thread_data) {
		g_print("%s: failure while allocating thread data.\n",ct->thread_name);
		return;
	}

	xstate = ct->thread_data;
	xstate->xmpp_gmaincontext = g_main_context_new();
	xstate->xmpp_gmainloop = g_main_loop_new(xstate->xmpp_gmaincontext, FALSE);

	/* Timeout source */
	xstate->xmpp_gtimeout = g_timeout_source_new_seconds(2);
	g_source_set_callback(xstate->xmpp_gtimeout, xmpp_pass_message, ct, NULL);
	xstate->xmpp_gtimeout_tag = g_source_attach(xstate->xmpp_gtimeout, xstate->xmpp_gmaincontext);

	return;
}

gpointer xmpp_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	g_print("XMPP started.\n");
	g_usleep(5*G_USEC_PER_SEC);
	g_print("%s: entering main loop\n",ct->thread_name);
	g_main_loop_run(xstate->xmpp_gmainloop);
	return data;
}

void xmpp_thread_deinit(gpointer data) {
	g_print("XMPP deinit.\n");
	return;
}

gboolean xmpp_pass_message(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	g_print("%s: tack %d\n", ct->thread_name, xstate->testval);
	xstate->testval++;

	if (xstate->testval == 2) {
		g_print("%s: asking main loop to exit.\n", ct->thread_name);
		g_main_loop_quit(ct->agh_mainloop);
		g_main_loop_quit(xstate->xmpp_gmainloop);
	}
	return TRUE;
}
