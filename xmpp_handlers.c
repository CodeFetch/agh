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
	//g_print("XMPP SENDTEXT handler was invoked.\n");

	if (m->msg_type == MSG_SENDTEXT) {
		g_print("Enqueuing text %s;\n",csp->text);
		g_queue_push_tail(xstate->outxmpp_messages, g_strdup(csp->text));
	}

	return NULL;
}

gpointer xmpp_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct command *cmd;
	struct agh_thread *ct;
	struct xmpp_state __attribute__((unused)) *xstate;
	struct agh_message __attribute__((unused)) *answer;

	ct = h->handler_data;
	xstate = ct->thread_data;
	answer = NULL;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	//g_print("XMPP: CMD SEEN. Current JID is %s\n", xmpp_conn_get_jid(xstate->xmpp_conn));

	/* quit */
	if (!g_strcmp0(cmd_get_operation(cmd), AGH_CMD_QUIT)) {
		g_main_loop_quit(ct->evl);
	}

	/*
	cmd_answer_prepare(cmd);
	cmd_answer_set_status(cmd, 100);
	answer = cmd_answer_msg(cmd, ct->comm, ct->agh_comm);
	msg_send(answer);
	*/

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
