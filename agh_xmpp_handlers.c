#include "agh_handlers.h"
#include "agh_xmpp.h"
#include "agh_xmpp_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct text_csp *csp;
	struct agh_state *mstate;
	struct xmpp_state *xstate;

	mstate = h->handler_data;
	xstate = mstate->xstate;

	csp = m->csp;

	if (m->msg_type == MSG_SENDTEXT) {
		g_print("Enqueuing text %s;\n",csp->text);
		g_queue_push_tail(xstate->outxmpp_messages, g_strdup(csp->text));
	}

	return NULL;
}

gpointer xmpp_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct command __attribute__((unused)) *cmd;
	struct agh_state *mstate;
	struct xmpp_state __attribute__((unused)) *xstate;

	mstate = h->handler_data;
	xstate = mstate->xstate;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	/* to be implemented */
	return NULL;
}
