#ifndef __agh_xmpp_h__
#define __agh_xmpp_h__
#include <strophe.h>
#include "agh.h"

#define AGH_XMPP_MAX_OUTGOING_QUEUED_MESSAGES 30

#define AGH_XMPP_RUN_ONCE_INTERVAL 200

#define AGH_XMPP_TCP_KEEPALIVE_TIMEOUT 60
#define AGH_XMPP_TCP_KEEPALIVE_INTERVAL 1

struct xmpp_state {
	xmpp_ctx_t *xmpp_ctx;
	xmpp_conn_t *xmpp_conn;
	xmpp_log_t *xmpp_log;
	gchar *jid;
	gchar *pass;
	GSource *xmpp_evs;
	guint xmpp_evs_tag;
	GQueue *outxmpp_messages;
	guint64 msg_id;
	guint xmpp_idle_state;
};

gpointer agh_xmpp_init(gpointer data);
void agh_xmpp_deinit(gpointer data);
gboolean xmpp_idle(gpointer data);

/* libstrophe XMPP handlers */
int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
void xmpp_connection_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata);

void agh_xmpp_send_out_messages(gpointer data);
void discard_xmpp_messages(gpointer data, gpointer userdata);

void xmpp_set_handlers_ext(struct agh_state *mstate);

#endif
