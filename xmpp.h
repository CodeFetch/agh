#ifndef __xmpp_h__
#define __xmpp_h__
#include <strophe.h>
#include "agh.h"

struct xmpp_state {
	xmpp_ctx_t *xmpp_ctx;
	xmpp_conn_t *xmpp_conn;
	xmpp_log_t *xmpp_log;
	char *jid;
	char *pass;
	GSource *xmpp_evs;
	guint xmpp_evs_tag;
	struct agh_thread *ct;
};

void xmpp_thread_init(gpointer data);
gpointer xmpp_thread_start(gpointer data);
void xmpp_thread_deinit(gpointer data);
gboolean xmpp_idle(gpointer data);

/* libstrophe XMPP handlers */
int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
void xmpp_connection_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata);

static struct agh_thread xmpp_thread_ops = {
	.thread_name = "XMPP",
	.agh_thread_init = xmpp_thread_init,
	.agh_thread_main = xmpp_thread_start,
	.agh_thread_deinit = xmpp_thread_deinit,
	.on_stack = 1
};
#endif
