#ifndef __xmpp_handlers_h__
#define __xmpp_handlers_h__

void xmpp_sendmsg_handler_init(gpointer data);
gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage);
void xmpp_sendmsg_handler_finalize(gpointer data);

static struct handler xmpp_sendmsg_handler = {
	.enabled = TRUE,
	.on_stack = TRUE,
	.handler_initialize = xmpp_sendmsg_handler_init,
	.handle = xmpp_sendmsg_handle,
	.handler_finalize = xmpp_sendmsg_handler_finalize,
};

#endif
