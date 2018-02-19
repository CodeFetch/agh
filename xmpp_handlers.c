#include "handlers.h"
#include "xmpp_handlers.h"
#include "messages.h"

void xmpp_test_handler_init(gpointer data, gpointer hsd) {
	struct handler *h = data;

	/* We are not allocating memory because we are only assigning a pointer value to another; otherwise,other considerations are needed */
	h->handler_data = hsd;
	// XXX why sizeof(*hsd) == 1?
	g_print("XMPP test handler init request.\n");
	return;
}

void xmpp_test_handler_finalize(gpointer data, gpointer hsd) {
	g_print("XMPP test handler finalize request.\n");
	return;
}

gpointer xmpp_test_handle(gpointer data, gpointer hmessage) {
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

gpointer xmpp_core_test_handle(gpointer data, gpointer hmessage) {
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
