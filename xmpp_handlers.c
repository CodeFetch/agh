#include "handlers.h"
#include "xmpp.h"
#include "xmpp_handlers.h"
#include "messages.h"

void xmpp_sendmsg_handler_init(gpointer data) {
	return;
}

void xmpp_sendmsg_handler_finalize(gpointer data) {
	return;
}

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct text_csp *csp;
	struct xmpp_state *xstate = h->ext_data;

	csp = m->csp;

	g_print("Enqueuing text %s;\n",csp->text);
	g_queue_push_tail(xstate->outxmpp_messages, csp->text);

	return NULL;
}
