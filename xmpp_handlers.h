#ifndef __xmpp_handlers_h__
#define __xmpp_handlers_h__

gpointer xmpp_sendmsg_handle(gpointer data, gpointer hmessage);
gpointer xmpp_cmd_handle(gpointer data, gpointer hmessage);
gpointer xmpp_event_handle(gpointer data, gpointer hmessage);

#endif
