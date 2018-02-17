#ifndef __xmpp_handlers_h__
#define __xmpp_handlers_h__

void xmpp_test_handler_init(gpointer data, gpointer hsd);
gpointer xmpp_test_handle(gpointer data, gpointer hmessage);
gpointer xmpp_core_test_handle(gpointer data, gpointer hmessage);
void xmpp_test_handler_finalize(gpointer data, gpointer hsd);

static struct handler xmpp_test_handler = {
	.enabled = TRUE,
	.on_stack = TRUE,
	.handler_initialize = xmpp_test_handler_init,
	.handle = xmpp_test_handle,
	.handler_finalize = xmpp_test_handler_finalize,
};

static struct handler xmpp_core_test_handler = {
	.enabled = TRUE,
	.on_stack = TRUE,
	.handler_initialize = xmpp_test_handler_init,
	.handle = xmpp_core_test_handle,
	.handler_finalize = xmpp_test_handler_finalize,
};

#endif
