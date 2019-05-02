#ifndef __agh_xmpp_h__
#define __agh_xmpp_h__
#include <strophe.h>
#include "agh.h"

#define AGH_XMPP_MAX_OUTGOING_QUEUED_MESSAGES 300

#define AGH_XMPP_RUN_ONCE_INTERVAL 10

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
	GQueue *controllers;
	GSource *xmpp_evs;
	guint xmpp_evs_tag;

	/* Config. */
	struct uci_context *uci_ctx;
	struct uci_package *xpackage;
	struct uci_section *xsection;
	gint ping_interval;
	gint ping_timeout;

	/* state */
	gboolean ping_is_timeout;
	guint xmpp_idle_state;
	GQueue *outxmpp_messages;
	guint64 msg_id;
	struct agh_xmpp_caps_entity *e;
	guint failing;
	gboolean failed_flag;
};

struct xmpp_csp {
	gchar *to;
	gchar *from;
	gchar *text;
	gchar *id;
};

gint agh_xmpp_init(struct agh_state *mstate);
gint agh_xmpp_deinit(struct agh_state *mstate);

void discard_xmpp_messages(gpointer data, gpointer userdata);

gint agh_xmpp_free_csp(struct xmpp_csp *c);

#endif
