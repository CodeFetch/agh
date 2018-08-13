#ifndef __agh_xmpp_h__
#define __agh_xmpp_h__
#include <strophe.h>
#include "agh.h"

#define AGH_XMPP_MAX_OUTGOING_QUEUED_MESSAGES 30

#define AGH_XMPP_RUN_ONCE_INTERVAL 200

/* Delivery receipts. */
#define AGH_XMPP_FEATURE_RECEIPTS "urn:xmpp:receipts"

#define AGH_XMPP_STANZA_NS_RECEIPTS AGH_XMPP_FEATURE_RECEIPTS

/* XMPP stanzas names, attributes and so on. */
#define AGH_XMPP_STANZA_NAME_C "c"
#define AGH_XMPP_STANZA_NS_CAPS "http://jabber.org/protocol/caps"
#define AGH_XMPP_STANZA_ATTR_HASH "hash"
#define AGH_XMPP_STANZA_NAME_QUERY "query"
#define AGH_XMPP_STANZA_TYPE_RESULT "result"
#define AGH_XMPP_STANZA_ATTR_NODE "node"
#define AGH_XMPP_STANZA_ATTR_VAR "var"
#define AGH_XMPP_STANZA_ATTR_VER "ver"
#define AGH_XMPP_STANZA_NAME_FEATURE "feature"
#define AGH_XMPP_STANZA_ATTR_VAR "var"
#define AGH_XMPP_STANZA_NS_PING "urn:xmpp:ping"
#define AGH_XMPP_STANZA_TYPE_GET "get"
#define AGH_XMPP_STANZA_NAME_PING "ping"
/* End of XMPP stanza attributes. */

/* Config. */

/* Mandatory options. */
#define AGH_XMPP_UCI_PACKAGE "agh_xmpp"
#define AGH_XMPP_UCI_SECTION_TYPE "xmpp_connection"
#define AGH_XMPP_UCI_SECTION_NAME "connection"
#define AGH_XMPP_UCI_OPTION_NODE "node"
#define AGH_XMPP_UCI_OPTION_DOMAIN "domain"
#define AGH_XMPP_UCI_OPTION_RESOURCE "resource"
#define AGH_XMPP_UCI_OPTION_PASS "password"
#define AGH_XMPP_UCI_OPTION_KA_INTERVAL "tcp_keepalive_interval"
#define AGH_XMPP_UCI_OPTION_KA_TIMEOUT "tcp_keepalive_timeout"
#define AGH_XMPP_UCI_OPTION_XMPP_PING_INTERVAL "ping_interval"
#define AGH_XMPP_UCI_OPTION_XMPP_PING_TIMEOUT "ping_timeout"
#define AGH_XMPP_UCI_OPTION_CONTROLLER "controller_barejid"

/* Other options. */
#define AGH_XMPP_UCI_OPTION_ALTDOMAIN "altdomain"
#define AGH_XMPP_UCI_OPTION_ALTPORT "altport"

/* Ping states. */
#define AGH_XMPP_PING_STATE_INACTIVE 0
#define AGH_XMPP_PING_STATE_WAITING 1
#define AGH_XMPP_PING_STATE_SENT 2

struct xmpp_state {
	xmpp_ctx_t *xmpp_ctx;
	xmpp_conn_t *xmpp_conn;
	xmpp_log_t *xmpp_log;
	gchar *jid;
	const gchar *controller;
	GSource *xmpp_evs;
	guint xmpp_evs_tag;
	GQueue *outxmpp_messages;
	guint64 msg_id;
	guint xmpp_idle_state;
	struct agh_xmpp_caps_entity *e;

	/* Config. */
	struct uci_context *uci_ctx;
	struct uci_package *xpackage;
	struct uci_section *xsection;
	gint ping_interval;
	gint ping_timeout;
	gboolean ping_is_timeout;
};

struct xmpp_csp {
	gchar *to;
	gchar *from;
	gchar *text;
	gchar *id;
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
void agh_xmpp_prepare_entity(struct xmpp_state *xstate);
int discoinfo_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
void agh_xmpp_config_init(struct agh_state *mstate);
void agh_xmpp_start_statemachine(struct agh_state *mstate);
const gchar *agh_xmpp_getoption(struct xmpp_state *xstate, gchar *name);
void agh_xmpp_conn_setup(struct agh_state *mstate, const gchar *node, const gchar *domain, const gchar *resource, const gchar *pass, gint ka_interval, gint ka_timeout);
int pong_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
struct agh_message *agh_xmpp_new_message(const gchar *from, const gchar *to, const gchar *id, gchar *text);
xmpp_stanza_t *agh_xmpp_build_ping_base(struct agh_state *mstate, const gchar *to, const gchar *id, gchar *type);
int ping_handler(xmpp_conn_t *const conn, void *const userdata);
int ping_timeout_handler(xmpp_conn_t *const conn, void *const userdata);
int iq_result_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

#endif
