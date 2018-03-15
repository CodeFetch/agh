#ifndef __xmpp_handlers_h__
#define __xmpp_handlers_h__

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage);

static struct handler xmpp_sendmsg_handler = {
	.enabled = TRUE,
	.on_stack = TRUE,
	.handler_initialize = NULL,
	.handle = xmpp_sendmsg_handle,
	.handler_finalize = NULL,
};

#endif
