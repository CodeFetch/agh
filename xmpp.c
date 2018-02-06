#include <glib.h>
#include "agh.h"
#include "xmpp.h"
#include "aghservices.h"

void xmpp_thread_init(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate;

	/* Should a memory allocation failure occur, glib will terminate the application. */
	ct->thread_data = g_malloc0(sizeof(struct xmpp_state));

	xstate = ct->thread_data;

	aghservices_messaging_setup(ct);

	return;
}

gpointer xmpp_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	g_print("XMPP started.\n");
	g_usleep(8*G_USEC_PER_SEC);
	g_print("%s: entering main loop\n",ct->thread_name);
	g_main_loop_run(ct->evl);
	return data;
}

void xmpp_thread_deinit(gpointer data) {
	struct agh_thread *ct = data;
	g_print("XMPP deinit.\n");
	g_free(ct->thread_data);
	return;
}
