#include "handlers.h"
#include "xmpp.h"
#include "xmpp_handlers.h"
#include "messages.h"

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct text_csp *csp;
	struct agh_thread *ct;
	struct xmpp_state *xstate;

	ct = h->handler_data;
	xstate = ct->thread_data;

	csp = m->csp;

	g_print("Enqueuing text %s;\n",csp->text);
	g_queue_push_tail(xstate->outxmpp_messages, csp->text);

	return NULL;
}
