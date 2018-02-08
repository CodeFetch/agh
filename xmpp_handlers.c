#include "callbacks.h"
#include "xmpp_handlers.h"
#include "messages.h"

void xmpp_test_handler_init(gpointer data) {
	struct handler *h = data;

	g_print("XMPP test handler init request.\n");
	return;
}

void xmpp_test_handler_finalize(gpointer data) {
	g_print("XMPP test handler finalize request.\n");
	return;
}

gpointer xmpp_test_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct test_csp *mycsp = m->csp;

	g_print("XMPP test handler: message handled (%d).",mycsp->num);
	return NULL;
}
