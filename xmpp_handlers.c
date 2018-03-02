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
	struct text_csp *csp;
	struct xmpp_handler_data *xh = h->handler_data;

	csp = m->csp;

	g_print("Enqueuing text %s;\n",csp->text);
	g_queue_push_tail(xh->outxmpp_messages, csp->text);

	return NULL;
}
