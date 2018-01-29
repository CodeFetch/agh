#ifndef __xmpp_h__
#define __xmpp_h__
#include "agh.h"

struct xmpp_state {
	int x;
};

void xmpp_thread_init(gpointer data);
gpointer xmpp_thread_start(gpointer data);
void xmpp_thread_deinit(gpointer data);

struct agh_thread xmpp_thread_ops = {
	.thread_name = "XMPP",
	.agh_thread_init = xmpp_thread_init,
	.agh_thread_main = xmpp_thread_start,
	.agh_thread_deinit = xmpp_thread_deinit
};
#endif
