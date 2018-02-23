#include "handlers.h"
#include "xmpp_handlers.h"
#include "messages.h"

void xmpp_sendmsg_handler_init(gpointer data) {
	struct handler *h = data;
	struct xmpp_handler_data *xh;

	h->handler_data = g_malloc0(sizeof(struct xmpp_handler_data));
	xh = h->handler_data;

	xh->outxmpp_messages = g_queue_new();

	return;
}

void xmpp_sendmsg_handler_finalize(gpointer data) {
	guint num_undelivered_messages;
	struct handler *h = data;
	struct xmpp_handler_data *xh = h->handler_data;

	num_undelivered_messages = g_queue_get_length(xh->outxmpp_messages);
	if (num_undelivered_messages) {
		g_print("XMPP handler: losing %d pending messages. this should not happen; leaking memory.\n",num_undelivered_messages);
	}
	g_queue_free_full(xh->outxmpp_messages, g_free);
	g_free(h->handler_data);
	xh->outxmpp_messages = NULL;
	h->handler_data = NULL;
	return;
}

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct test_csp *mycsp = m->csp;
	struct agh_message *answer;
	struct test_csp *answer_csp;
	struct agh_thread *ct = h->handler_data;

	answer = msg_alloc(sizeof(struct test_csp));
	answer_csp = answer->csp;

	g_print("XMPP test handler: handling message (%d).\n",mycsp->num);
	if (mycsp->num > 10) {
		g_print("XMPP thread will exit.\n");
		g_main_loop_quit(ct->evl);
	}
	answer_csp->num = ++mycsp->num;
	g_print("XMPP Handler exiting.\n");
	return answer;
}

gpointer xmpp_core_sendmsg_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct test_csp *mycsp = m->csp;
	struct agh_message *answer;
	struct test_csp *answer_csp;
	struct agh_state *mstate = h->handler_data;

	answer = msg_alloc(sizeof(struct test_csp));
	answer_csp = answer->csp;

	g_print("CORE XMPP test handler: handling message (%d).\n",mycsp->num);
	answer_csp->num = ++mycsp->num;

	if (answer_csp->num > 10) {
		g_main_loop_quit(mstate->agh_mainloop);
		g_print("Bye bye!\n");
	}

	return answer;
}
