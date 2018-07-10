#include "handlers.h"
#include "xmpp.h"
#include "xmpp_handlers.h"
#include "messages.h"
#include "commands.h"

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct text_csp *csp;
	struct agh_thread *ct;
	struct xmpp_state *xstate;

	ct = h->handler_data;
	xstate = ct->thread_data;

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
	struct agh_thread *ct;
	struct xmpp_state __attribute__((unused)) *xstate;

	ct = h->handler_data;
	xstate = ct->thread_data;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	/* to be implemented */
	return NULL;
}

gpointer xmpp_event_handle(gpointer data, gpointer hmessage) {
	struct command __attribute__((unused)) *event;
	struct handler *h = data;
	struct agh_thread *ct = h->handler_data;
	struct agh_message *m = hmessage;

	event = m->csp;

	if (m->msg_type != MSG_EVENT)
		return NULL;

	//g_print("%s: an event has been intercepted.\n\t(operation is %s, arg1 is %s, arg2 is %s, arg3 is %s and arg4 is %s)",ct->thread_name,event_name(event),event_arg(event, 1),event_arg(event, 2),event_arg(event, 3),event_arg(event, 4));
	return NULL;
}

gpointer xmpp_exit_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_thread *ct = h->handler_data;
	struct agh_message *m = hmessage;
	struct xmpp_state *xstate = ct->thread_data;

	if (m->msg_type != MSG_EXIT)
		return NULL;

	xstate->exit = TRUE;
	return NULL;
}
