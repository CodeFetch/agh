#include <glib.h>
#include <uci.h>
#include <errno.h>
#include "agh_xmpp.h"
#include "agh.h"
#include "agh_logging.h"
#include "agh_handlers.h"
#include "agh_xmpp_handlers.h"
#include "agh_messages.h"
#include "agh_xmpp_caps.h"
#include "agh_commands.h"

/* Log messages from AGH_LOG_DOMAIN_XMPP domain. */
#define AGH_LOG_DOMAIN_XMPP	"XMPP"

/* Logging macros. */
#define agh_log_xmpp_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_XMPP, message, ##__VA_ARGS__)
#define agh_log_xmpp_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_XMPP, message, ##__VA_ARGS__)

static const gchar *agh_xmpp_getoption(struct xmpp_state *xstate, gchar *name) {
	gchar *content;
	struct uci_option *option;

	content = NULL;

	if (!xstate || !xstate->uci_ctx || !xstate->xsection || !name) {
		agh_log_xmpp_crit("invoked with no XMPP state, no UCI context, no config section (xsection) or NULL name parameter");
		return content;
	}

	option = uci_lookup_option(xstate->uci_ctx, xstate->xsection, name);

	if ((!option) || (option->type != UCI_TYPE_STRING)) {
		agh_log_xmpp_dbg("option %s was not found, or was not an UCI string", name);
		return content;
	}

	content = option->v.string;

	/* can this happen ? */
	if (!content)
		agh_log_xmpp_crit("returning NULL content from UCI");

	return content;
}

static GQueue *agh_xmpp_getoption_list(struct xmpp_state *xstate, gchar *name) {
	struct uci_option *option;
	GQueue *res;
	struct uci_element *e;

	res = NULL;

	if (!xstate || !xstate->uci_ctx || !xstate->xsection || !name) {
		agh_log_xmpp_crit("XMPP state or needed pointer missing");
		return res;
	}

	option = uci_lookup_option(xstate->uci_ctx, xstate->xsection, name);

	if ((!option) || (option->type != UCI_TYPE_LIST)) {
		agh_log_xmpp_dbg("option %s was not found, or was not an UCI list", name);
		return res;
	}

	res = g_queue_new();

	uci_foreach_element(&option->v.list, e) {
		g_queue_push_tail(res, e->name);
	}

	return res;
}

/* this function is a libstrophe handler */
static int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	xmpp_stanza_t *reply, *query, *name, *version, *text;
	const char *ns;
	struct agh_state *mstate;
	struct xmpp_state *xstate;
	xmpp_ctx_t *ctx;
	gint retval;

	mstate = userdata;

	reply = NULL;
	query = NULL;
	retval = 1;

	xstate = mstate->xstate;
	ctx = xstate->xmpp_ctx;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_dbg("exiting due to xstate->xmpp_idle_state != 1");
		goto out;
	}

	agh_log_xmpp_dbg("received version request from %s", xmpp_stanza_get_from(stanza));

	reply = xmpp_stanza_reply(stanza);
	if (!reply) {
		retval++;
		goto out;
	}

	xmpp_stanza_set_type(reply, "result");

	query = xmpp_stanza_new(ctx);
	if (!query) {
		retval++;
		goto out;
	}

	xmpp_stanza_set_name(query, "query");
	ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));

	if (ns) {
		xmpp_stanza_set_ns(query, ns);
	}

	name = xmpp_stanza_new(ctx);
	if (!name) {
		retval++;
		goto out;
	}

	xmpp_stanza_set_name(name, "name");
	xmpp_stanza_add_child(query, name);
	xmpp_stanza_release(name);

	text = xmpp_stanza_new(ctx);
	if (!text) {
		retval++;
		goto out;
	}
	xmpp_stanza_set_text(text, "AGH ("AGH_RELEASE_NAME")");
	xmpp_stanza_add_child(name, text); /* this should not be an user-after-free, due to the fact that now name is linked to "query" */
	xmpp_stanza_release(text);

	version = xmpp_stanza_new(ctx);
	if (!version) {
		retval++;
		goto out;
	}
	xmpp_stanza_set_name(version, "version");
	xmpp_stanza_add_child(query, version);
	xmpp_stanza_release(version);

	text = xmpp_stanza_new(ctx);
	if (!text) {
		retval++;
		goto out;
	}
	xmpp_stanza_set_text(text, AGH_VERSION);
	xmpp_stanza_add_child(version, text);
	xmpp_stanza_release(text);

	xmpp_stanza_add_child(reply, query);
	xmpp_stanza_release(query);
	xmpp_send(conn, reply);
	xmpp_stanza_release(reply);

out:
	if (retval > 1) {
		if (reply)
			xmpp_stanza_release(reply);

		if (query)
			xmpp_stanza_release(query);
	}

	return 1;
}

void discard_xmpp_messages(gpointer data, gpointer userdata) {
	struct xmpp_state *xstate = userdata;
	struct agh_message *artificial_message = data;
	struct agh_text_payload *tcsp;

	if (!artificial_message) {
		agh_log_xmpp_crit("NULL message was passed");
		return;
	}

	tcsp = artificial_message->csp;

	agh_log_xmpp_crit("[%s]: %s",tcsp->source_id ? tcsp->source_id : "unknown source", tcsp->text ? tcsp->text : "unknown text?");

	g_queue_remove(xstate->outxmpp_messages, artificial_message);

	agh_msg_dealloc(artificial_message);

	return;
}

static gint xmpp_set_handlers_ext(struct agh_state *mstate) {
	struct agh_handler *xmpp_sendmsg_handler;
	struct agh_handler *xmpp_cmd_handler;
	gint retval;

	retval = 1;
	xmpp_sendmsg_handler = NULL; /* useless */
	xmpp_cmd_handler = NULL;     /* not useless */

	if ( !(xmpp_sendmsg_handler = agh_new_handler("xmpp_sendmsg_handler")) )
		goto out;

	agh_handler_set_handle(xmpp_sendmsg_handler, xmpp_sendmsg_handle);
	agh_handler_enable(xmpp_sendmsg_handler, TRUE);

	if ( !(xmpp_cmd_handler = agh_new_handler("xmpp_cmd_handler")) )
		goto out;

	agh_handler_set_handle(xmpp_cmd_handler, xmpp_cmd_handle);
	agh_handler_enable(xmpp_cmd_handler, TRUE);

	agh_handler_register(mstate->agh_handlers, xmpp_sendmsg_handler);
	agh_handler_register(mstate->agh_handlers, xmpp_cmd_handler);
	retval = 0;

out:
	if (retval) {
		g_clear_pointer(&xmpp_sendmsg_handler, g_free);
		g_clear_pointer(&xmpp_cmd_handler, g_free);
	}

	return retval;;
}

static gint agh_xmpp_prepare_entity(struct xmpp_state *xstate) {
	gint id;
	gint retval;

	id = 0;
	retval = 0;

	if (!xstate || xstate->e) {
		agh_log_xmpp_dbg("No XMPP state, or CAPS entity already present");
		retval = 61;
		goto out_badxstate;
	}

	xstate->e = agh_xmpp_caps_entity_alloc();
	if (xstate->e) {

		id = agh_xmpp_caps_add_entity(xstate->e);
		if (id < 0) {
			retval = id;
			goto out;
		}

		if ( (retval = agh_xmpp_caps_set_entity_data(xstate->e, id, "AGH "AGH_VERSION" ("AGH_RELEASE_NAME")", "client", "bot", NULL)) ) {
			agh_log_xmpp_crit("agh_xmpp_caps_set_entity_data failed (code=%" G_GINT16_FORMAT")", retval);
			goto out;
		}

		retval = agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_FEATURE_RECEIPTS);
		if (retval < 0)
			goto out;

		retval = agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_STANZA_NS_CAPS);
		if (retval < 0)
			goto out;

		retval = agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_STANZA_NS_PING);
		if (retval < 0)
			goto out;

		retval = 0; /* because, after all, agh_xmpp_caps_add_feature returns "IDs", have a look at it... */

	}
	else
		retval = 2;

out:
	if (retval && xstate->e) {
		agh_xmpp_caps_entity_dealloc(xstate->e);
		xstate->e = NULL;
		agh_log_xmpp_dbg("xstate->e is now NULL; have a good day!");
	}
out_badxstate:
	return retval;
}

/* libstrophe handler */
static int discoinfo_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_ctx_t *ctx = xstate->xmpp_ctx;

	const gchar *from;
	const gchar *srcnode;
	xmpp_stanza_t *incoming_query;
	xmpp_stanza_t *response;
	xmpp_stanza_t *capsdata;
	gint retv;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_crit("exiting due to xstate->xmpp_idle_state != 1");
		return 1;
	}

	incoming_query = xmpp_stanza_get_child_by_name(stanza, AGH_XMPP_STANZA_NAME_QUERY);
	if (!incoming_query) {
		agh_log_xmpp_crit(AGH_XMPP_STANZA_NAME_QUERY" child stanza not found");
		return 1;
	}

	from = xmpp_stanza_get_from(stanza);

	if (!from) {
		agh_log_xmpp_crit("xmpp_stanza_get_from failed");
		return 1;
	}

	response = xmpp_iq_new(ctx, AGH_XMPP_STANZA_TYPE_RESULT, xmpp_stanza_get_id(stanza));
	if (!response) {
		agh_log_xmpp_crit("failure while allocating an "AGH_XMPP_STANZA_TYPE_RESULT" stanza");
		return 1;
	}

	xmpp_stanza_set_to(response, from);

	srcnode = xmpp_stanza_get_attribute(incoming_query, AGH_XMPP_STANZA_ATTR_NODE);

	retv = agh_xmpp_prepare_entity(xstate);
	if (retv) {
		xmpp_stanza_release(response);
		return 1;
	}

	capsdata = agh_xmpp_caps_get_capsdata(xstate);
	agh_xmpp_caps_entity_dealloc(xstate->e);
	xstate->e = NULL;
	if (!capsdata) {
		xmpp_stanza_release(response);
		return 1;
	}

	if (srcnode)
		xmpp_stanza_set_attribute(capsdata, AGH_XMPP_STANZA_ATTR_NODE, srcnode);

	xmpp_stanza_add_child(response, capsdata);
	xmpp_stanza_release(capsdata);

	xmpp_stanza_set_from(response, xmpp_conn_get_bound_jid(conn));

	xmpp_send(conn, response);
	xmpp_stanza_release(response);

	return 1;
}

static xmpp_stanza_t *agh_xmpp_build_ping_base(struct agh_state *mstate, const gchar *to, const gchar *id, gchar *stanza_type) {
	struct xmpp_state *xstate =mstate->xstate;
	xmpp_stanza_t *iq_ping;
	xmpp_stanza_t *ping;
	gchar *msg_id;

	iq_ping = NULL;

	if (!mstate || !mstate->xstate || !stanza_type) {
		agh_log_xmpp_crit("bad parameters");
		return iq_ping;
	}

	if (id)
		msg_id = g_strdup(id);
	else {
		if (xstate->msg_id == G_MAXUINT64)
			xstate->msg_id = 0;

		msg_id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);
		xstate->msg_id++;
	}

	iq_ping = xmpp_iq_new(xstate->xmpp_ctx, stanza_type, msg_id);
	g_free(msg_id);
	if (!iq_ping) {
		agh_log_xmpp_crit("%s stanza allocation failed",stanza_type);
		return iq_ping;
	}

	if (g_strcmp0(stanza_type, AGH_XMPP_STANZA_TYPE_RESULT)) {
		ping = xmpp_stanza_new(xstate->xmpp_ctx);
		if (!ping) {
			agh_log_xmpp_crit("ping allocation failure");
			xmpp_stanza_release(iq_ping);
			iq_ping = NULL;
			return iq_ping;
		}

		xmpp_stanza_set_name(ping, AGH_XMPP_STANZA_NAME_PING);
		xmpp_stanza_set_ns(ping, AGH_XMPP_STANZA_NS_PING);
		xmpp_stanza_add_child(iq_ping, ping);
		xmpp_stanza_release(ping);
	}

	xmpp_stanza_set_from(iq_ping, xmpp_conn_get_bound_jid(xstate->xmpp_conn));

	if (to)
		xmpp_stanza_set_to(iq_ping, to);

	return iq_ping;
}

static struct agh_message *agh_xmpp_new_message(const gchar *from, const gchar *to, const gchar *id, gchar *text) {
	struct agh_message *m;
	struct xmpp_csp *xcsp;
	gint ret;

	m = NULL;

	if (!from || !id || !text) {
		agh_log_xmpp_crit("mandatory parameters are missing, but you may try specifying a NULL to");
		return m;
	}

	m = agh_msg_alloc();
	if (!m) {
		agh_log_xmpp_crit("AGH message allocation failure");
		return m;
	}

	xcsp = g_try_malloc0(sizeof(*xcsp));
	if (!xcsp) {
		agh_log_xmpp_crit("xmpp_csp structure allocation failed");
		ret = agh_msg_dealloc(m);
		if (ret)
			agh_log_xmpp_crit("failure while deallocating message (code=%" G_GINT16_FORMAT")", ret);

	}

	m->csp = xcsp;

	xcsp->text = text;
	xcsp->from = g_strdup(from);

	if (to)
		xcsp->to = g_strdup(to);

	if (id)
		xcsp->id = g_strdup(id);

	m->msg_type = MSG_XMPPTEXT;

	return m;
}

/* libstrophe handler */
static int ping_timeout_handler(xmpp_conn_t *const conn, void *const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_crit("xstate->xmpp_idle_state != 1");
		return 1;
	}

	agh_log_xmpp_crit("oops! Time to try reconnecting");

	xmpp_disconnect(xstate->xmpp_conn);

	return 0;
}

/* libstrophe handling */
static int ping_handler(xmpp_conn_t *const conn, void *const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_stanza_t *iq_ping;
	gchar *domain;

	iq_ping = NULL;
	domain = NULL;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_crit("xstate->xmpp_idle_state != 1");
		return 1;
	}

	domain = xmpp_jid_domain(xstate->xmpp_ctx, xmpp_conn_get_jid(xstate->xmpp_conn));
	if (!domain) {
		agh_log_xmpp_crit("xmpp_jid_domain failure");
		return 1;
	}

	iq_ping = agh_xmpp_build_ping_base(mstate, domain, NULL, AGH_XMPP_STANZA_TYPE_GET);
	if (!iq_ping) {
		xmpp_free(xstate->xmpp_ctx, domain);
		return 1;
	}

	xmpp_timed_handler_add(xstate->xmpp_conn, ping_timeout_handler, xstate->ping_timeout * 1000, mstate);
	xmpp_send(xstate->xmpp_conn, iq_ping);
	xmpp_stanza_release(iq_ping);
	xmpp_free(xstate->xmpp_ctx, domain);
	xstate->ping_is_timeout = TRUE;

	return 0;
}

/* libstrophe handler */
static int iq_result_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_crit("xstate->xmpp_idle_state != 1");
		return 1;
	}

	if (xstate->ping_interval && xstate->ping_is_timeout) {
		xmpp_timed_handler_delete(xstate->xmpp_conn, ping_timeout_handler);
		xmpp_timed_handler_add(xstate->xmpp_conn, ping_handler, xstate->ping_interval * 1000, mstate);
		xstate->ping_is_timeout = FALSE;
	}

	return 1;
}

gint agh_xmpp_free_csp(struct xmpp_csp *c) {
	gint retval;

	retval = 1;

	if (c) {
		g_free(c->from);
		g_free(c->to);
		g_free(c->text);
		g_free(c->id);
		g_free(c);
		retval = 0;
	}

	return retval;
}

/* libstrophe handler */
static int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct xmpp_state *xstate;
	xmpp_ctx_t *ctx;
	xmpp_stanza_t *body;
	xmpp_stanza_t *receipt_request;
	const gchar *type;
	gchar *intext;
	struct agh_message *m;
	struct agh_state *mstate;
	const gchar *from;
	const gchar *to;
	const gchar *msg_id;
	xmpp_stanza_t *receipt_message;
	xmpp_stanza_t *receipt_response;
	gchar *from_barejid;
	gboolean is_a_controller;
	guint i;
	guint controllers_queue_len;
	gchar *current_controller;
	gchar *receipt_response_id;

	mstate = userdata;
	xstate = mstate->xstate;
	ctx = xstate->xmpp_ctx;

	receipt_request = NULL;
	receipt_message = NULL;
	receipt_response = NULL;
	is_a_controller = FALSE;
	receipt_response_id = NULL;

	if (xstate->xmpp_idle_state != 1) {
		agh_log_xmpp_crit("exiting due to xstate->xmpp_idle_state != 1");
		return 1;
	}

	/* body should not be freed */
	body = xmpp_stanza_get_child_by_name(stanza, "body");
	if (!body) {
		agh_log_xmpp_crit("no body in this stanza");
		return 1;
	}

	/* If type is NULL or is an error, stop here; type should not be freed. */
	type = xmpp_stanza_get_type(stanza);
	if (!type || !g_strcmp0(type, "error")) {
		agh_log_xmpp_crit("type was NULL or an error");
		return 1;
	}

	from = xmpp_stanza_get_from(stanza);
	if (!from) {
		agh_log_xmpp_crit("no from");
		return 1;
	}

	msg_id = xmpp_stanza_get_id(stanza);
	if (!msg_id) {
		agh_log_xmpp_crit("message has no id");
		return 1;
	}

	from_barejid = xmpp_jid_bare(ctx, from);
	if (!from_barejid) {
		agh_log_xmpp_crit("failure from from_barejid");
		return 1;
	}

	controllers_queue_len = g_queue_get_length(xstate->controllers);
	for (i = 0; i<controllers_queue_len;i++) {
		current_controller = g_queue_peek_nth(xstate->controllers, i);
		if (current_controller && !g_strcmp0(from_barejid, current_controller)) {
			is_a_controller = TRUE;
			break;
		}
	}
	if (!is_a_controller) {
		xmpp_free(ctx, from_barejid);
		agh_log_xmpp_dbg("this message was not from a controller");
		return 1;
	}

	intext = xmpp_stanza_get_text(body);
	if (!intext) {
		agh_log_xmpp_crit("can not get stanza text");
		xmpp_free(ctx, from_barejid);
		return 1;
	}

	receipt_request = xmpp_stanza_get_child_by_ns(stanza, AGH_XMPP_STANZA_NS_RECEIPTS);

	if (receipt_request) {

		if (xstate->msg_id == G_MAXUINT64)
			xstate->msg_id = 0;

		receipt_response_id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);
		receipt_message = xmpp_message_new(ctx, NULL, from, receipt_response_id);

		if (!receipt_message) {
			agh_log_xmpp_crit("receipt message allocation failure");
			xmpp_free(ctx, from_barejid);
			g_free(receipt_response_id);
			xmpp_free(ctx, intext);
			return 1;
		}

		receipt_response = xmpp_stanza_new(ctx);
		if (!receipt_response) {
			agh_log_xmpp_crit("stanza allocation failure");
			xmpp_stanza_release(receipt_message);
			xmpp_free(ctx, from_barejid);
			xmpp_free(ctx, intext);
			g_free(receipt_response_id);
			return 1;
		}

		xmpp_stanza_set_name(receipt_response, "received");
		xmpp_stanza_set_ns(receipt_response, AGH_XMPP_STANZA_NS_RECEIPTS);
		xmpp_stanza_set_id(receipt_response, msg_id);
		xmpp_stanza_add_child(receipt_message, receipt_response);
		xmpp_stanza_release(receipt_response);
		xmpp_stanza_set_from(receipt_message, xmpp_conn_get_bound_jid(conn));

		xmpp_send(conn, receipt_message);
		xmpp_stanza_release(receipt_message);
		xstate->msg_id++;
		g_free(receipt_response_id);
	}

	to = xmpp_stanza_get_to(stanza);

	m = agh_xmpp_new_message(from, to, xmpp_stanza_get_id(stanza), intext);
	if (!m) {
		agh_log_xmpp_crit("failure allocating AGH message");
		xmpp_free(ctx, intext);
		xmpp_free(ctx, from_barejid);
	}

	if ( (i = agh_msg_send(m, mstate->comm, NULL)) ) {
		agh_log_xmpp_crit("unable to send received XMPP message to core (code=%" G_GINT16_FORMAT")", i);
		/* note - both m and mstate->comm should not be NULL here, so agh_msg_send is expected to deallocate the message */
	}

	xmpp_free(ctx, from_barejid);

	return 1;
}

/* libstrophe handler */
static void xmpp_connection_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata) {
	struct xmpp_state *xstate;
	xmpp_stanza_t *pres;
	xmpp_ctx_t *ctx;
	struct agh_state *mstate;
	gint retv;

	mstate = userdata;
	xstate = mstate->xstate;

	ctx = xstate->xmpp_ctx;

	switch(status) {
	case XMPP_CONN_CONNECT:
		pres = xmpp_presence_new(ctx);

		if (!pres) {
			agh_log_xmpp_crit("unable to allocate presence stanza");
			break;
		}

		retv = agh_xmpp_prepare_entity(xstate);
		if (retv) {
			xmpp_stanza_release(pres);
			break;
		}

		retv = agh_xmpp_caps_add_hash(ctx, xstate->e, pres);
		agh_xmpp_caps_entity_dealloc(xstate->e);
		xstate->e = NULL;
		if (retv) {
			agh_log_xmpp_crit("agh_xmpp_caps_add_hash failed (code=%" G_GINT16_FORMAT")", retv);
			xmpp_stanza_release(pres);
			break;
		}
		agh_log_xmpp_dbg("sending presence");
		xmpp_send(xstate->xmpp_conn, pres);
		xmpp_stanza_release(pres);
		break;
	case XMPP_CONN_DISCONNECT:
		xstate->xmpp_idle_state++;
		break;
	case XMPP_CONN_FAIL:
		agh_log_xmpp_crit("connection failed");
		xstate->xmpp_idle_state++;
		break;
	default:
		agh_log_xmpp_crit("unknown state");
	}

	return;
}

static gint agh_xmpp_send_message(struct agh_state *mstate, const gchar *to, const gchar *text) {
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_ctx_t *ctx = xstate->xmpp_ctx;

	xmpp_stanza_t *reply;
	gchar *id;
	const gchar *from;
	gchar *local_text;

	from = xmpp_conn_get_bound_jid(xstate->xmpp_conn);
	if ((xstate->xmpp_idle_state != 1) || !from || !to || !text) {
		agh_log_xmpp_crit("exiting early due to bad state or parameters");
		return 1;
	}

	if (xstate->msg_id == G_MAXUINT64)
		xstate->msg_id = 0;

	id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);

	reply = xmpp_message_new(ctx, "chat", to, id);
	if (!reply) {
		agh_log_xmpp_crit("unable to allocate chat stanza for sending message");
		g_free(id);
		return 1;
	}

	local_text = g_strdup(text);
	xmpp_message_set_body(reply, local_text);
	xmpp_stanza_set_from(reply, from);
	xmpp_send(xstate->xmpp_conn, reply);
	xmpp_stanza_release(reply);
	g_free(local_text);
	g_free(id);
	xstate->msg_id++;

	return 0;
}

static gint agh_xmpp_send_out_messages(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;
	struct agh_text_payload *tcsp;
	struct agh_message *artificial_message;
	gchar *agh_message_source_name;
	gchar *agh_message_source_from;
	guint i;
	guint controllers_queue_len;
	gchar *current_controller;
	gint retval;

	agh_message_source_from = NULL;
	agh_message_source_name = NULL;
	retval = 0;

	if (!xstate->outxmpp_messages)
		return 0;

	if (!g_queue_get_length(xstate->outxmpp_messages))
		return 0;

	/* Get a message */
	artificial_message = g_queue_pop_head(xstate->outxmpp_messages);
	if (!artificial_message) {
		agh_log_xmpp_crit("picked artificial_message is NULL");
		return 1;
	}

	/* We expect only MSG_SENDTEXT messages, or in any case, messages with a agh_text_payload. */
	tcsp = artificial_message->csp;

	if (!tcsp) {
		agh_log_xmpp_crit("NULL message payload");
		agh_msg_dealloc(artificial_message);
		return 1;
	}

	if (tcsp->source_id) {
		retval = agh_message_source(tcsp->source_id, &agh_message_source_name, &agh_message_source_from);
		if (retval) {
			agh_log_xmpp_crit("failure while parsing message sources (code=%" G_GINT16_FORMAT")", retval);
			agh_msg_dealloc(artificial_message);
			return retval;
		}

		if (agh_message_source_from) {
			if (!g_strcmp0(agh_message_source_name, "XMPP")) {
				retval = agh_xmpp_send_message(mstate, agh_message_source_from, tcsp->text);
				if (retval)
					agh_log_xmpp_crit("failure while sending message (code=%" G_GINT16_FORMAT")", retval);
			}

			g_free(agh_message_source_from);
			g_free(agh_message_source_name);
		}
	}
	else {
		controllers_queue_len = g_queue_get_length(xstate->controllers);
		for (i=0;i<controllers_queue_len;i++) {
			current_controller = g_queue_peek_nth(xstate->controllers, i);
			retval = agh_xmpp_send_message(mstate, current_controller, tcsp->text);
			if (retval) {
				agh_log_xmpp_crit("failure while sending message to all controllers (code=%" G_GINT16_FORMAT")", retval);
				break;
			}
		}
	}

	agh_msg_dealloc(artificial_message);

	return 0;
}

/* libstrophe handler */
static int pong_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	xmpp_stanza_t *iq_ping;

	iq_ping = NULL;

	g_print("%s: nice, I was invoked.\n",__FUNCTION__);

	if (xstate->xmpp_idle_state != 1) {
		g_print("%s: exiting due to xstate->xmpp_idle_state != 1\n",__FUNCTION__);
		return 1;
	}

	iq_ping = agh_xmpp_build_ping_base(mstate, xmpp_stanza_get_from(stanza), xmpp_stanza_get_id(stanza), AGH_XMPP_STANZA_TYPE_RESULT);
	xmpp_send(xstate->xmpp_conn, iq_ping);
	xmpp_stanza_release(iq_ping);
	if (xstate->ping_interval) {
		//g_print("%s: \"resetting\" ping logic due to ping from server\n",__FUNCTION__);

		xmpp_timed_handler_delete(xstate->xmpp_conn, ping_handler);
		xmpp_timed_handler_add(xstate->xmpp_conn, ping_handler, xstate->ping_interval * 1000, mstate);
		xstate->ping_is_timeout = FALSE;
	}

	return 1;
}

static void agh_xmpp_conn_setup(struct agh_state *mstate, const gchar *node, const gchar *domain, const gchar *resource, const gchar *pass, gint ka_interval, gint ka_timeout) {
	struct xmpp_state *xstate = mstate->xstate;
	gchar *jid;

	jid = NULL;

	if (xstate->xmpp_conn) {
		xmpp_conn_release(xstate->xmpp_conn);
		xstate->xmpp_conn = NULL;
	}

	if (xstate->xmpp_ctx) {
		if (xstate->jid)
			xmpp_free(xstate->xmpp_ctx, xstate->jid);

		xmpp_ctx_free(xstate->xmpp_ctx);
		xstate->xmpp_ctx = NULL;
	}

	xstate->xmpp_log = xmpp_get_default_logger(XMPP_LEVEL_INFO);

	/* First parameter is NULL since we don't provide our own memory allocator. */
	xstate->xmpp_ctx = xmpp_ctx_new(NULL, xstate->xmpp_log);
	if (!xstate->xmpp_ctx)
		return;

	jid = xmpp_jid_new(xstate->xmpp_ctx, node, domain, resource);
	if (!jid) {
		xmpp_ctx_free(xstate->xmpp_ctx);
		return;
	}

	xstate->xmpp_conn = xmpp_conn_new(xstate->xmpp_ctx);
	if (!xstate->xmpp_conn) {
		xmpp_ctx_free(xstate->xmpp_ctx);
		xmpp_free(xstate->xmpp_ctx, jid);
		return;
	}

	xmpp_conn_set_flags(xstate->xmpp_conn, XMPP_CONN_FLAG_MANDATORY_TLS);

	xmpp_handler_add(xstate->xmpp_conn, version_handler, "jabber:iq:version", "iq", NULL, mstate);
	xmpp_handler_add(xstate->xmpp_conn, discoinfo_handler, XMPP_NS_DISCO_INFO, "iq", NULL, mstate);
	xmpp_handler_add(xstate->xmpp_conn, message_handler, NULL, "message", NULL, mstate);
	xmpp_handler_add(xstate->xmpp_conn, pong_handler, AGH_XMPP_STANZA_NS_PING, "iq", AGH_XMPP_STANZA_TYPE_GET, mstate);
	xmpp_handler_add(xstate->xmpp_conn, iq_result_handler, NULL, "iq", AGH_XMPP_STANZA_TYPE_RESULT, mstate);

	xmpp_conn_set_jid(xstate->xmpp_conn, jid);
	xmpp_conn_set_pass(xstate->xmpp_conn, pass);

	if (ka_timeout && ka_interval)
		xmpp_conn_set_keepalive(xstate->xmpp_conn, ka_timeout, ka_interval);

	xstate->jid = jid;

	if (xstate->ping_timeout && xstate->ping_interval) {
			xmpp_timed_handler_add(xstate->xmpp_conn, ping_handler, xstate->ping_interval * 1000, mstate);
	}

	return;
}

static void agh_xmpp_config_init(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;
	struct uci_package *package;
	struct uci_section *section;
	struct uci_ptr ptr;
	const gchar *optval;

	const gchar *jid_node;
	const gchar *jid_domain;
	const gchar *jid_resource;
	const gchar *pass;
	gint ka_interval;
	gint ka_timeout;

	gchar *eptr;
	gint ping_timeout;
	gint ping_interval;
	GQueue *controllers;

	if (xstate->uci_ctx) {
		uci_unload(xstate->uci_ctx, xstate->xpackage);
		uci_free_context(xstate->uci_ctx);
		xstate->uci_ctx = NULL;
		xstate->xpackage = NULL;
		xstate->xsection = NULL;
	}

	xstate->uci_ctx = uci_alloc_context();
	if (!xstate->uci_ctx) {
		g_print("%s: can not allocate UCI context\n",__FUNCTION__);
		goto out_noctx;
	}

	xstate->uci_ctx->flags |= UCI_FLAG_STRICT;

	if (uci_lookup_ptr(xstate->uci_ctx, &ptr, AGH_XMPP_UCI_PACKAGE, FALSE) != UCI_OK) {
		g_print("%s: can not find "AGH_XMPP_UCI_PACKAGE" UCI package\n",__FUNCTION__);
		goto out;
	}
	package = ptr.p;

	/* Search for the section we are interested in. */
	section = uci_lookup_section(xstate->uci_ctx, package, AGH_XMPP_UCI_SECTION_NAME);
	if (!section) {
		g_print("%s: can not find "AGH_XMPP_UCI_SECTION_NAME" section\n",__FUNCTION__);
		goto out;
	}

	xstate->xpackage = package;
	xstate->xsection = section;

	/* Mandatory options. */
	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_NODE);
	if (!optval)
		goto out;

	jid_node = optval;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_DOMAIN);
	if (!optval)
		goto out;

	jid_domain = optval;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_RESOURCE);
	if (!optval)
		goto out;

	jid_resource = optval;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_PASS);
	if (!optval)
		goto out;

	pass = optval;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_KA_INTERVAL);
	if (!optval)
		goto out;

	ka_interval = strtol(optval, &eptr, 10);

	if (ka_interval < INT_MIN || ka_interval > INT_MAX)
		goto out;

	if (!(!eptr || *eptr == '\0'))
		goto out;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_KA_TIMEOUT);
	if (!optval)
		goto out;

	ka_timeout = strtol(optval, &eptr, 10);

	if (ka_timeout < INT_MIN || ka_timeout > INT_MAX)
		goto out;

	if (!(!eptr || *eptr == '\0'))
		goto out;

	controllers = agh_xmpp_getoption_list(xstate, AGH_XMPP_UCI_OPTION_CONTROLLER);
	if (!controllers)
		goto out;

	if (xstate->controllers)
		g_queue_free(xstate->controllers);

	xstate->controllers = controllers;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_XMPP_PING_TIMEOUT);

	ping_timeout = strtol(optval, &eptr, 10);

	if (ping_timeout < INT_MIN || ping_timeout > INT_MAX)
		goto out;

	if (!(!eptr || *eptr == '\0'))
		goto out;

	optval = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_XMPP_PING_INTERVAL);

	ping_interval = strtol(optval, &eptr, 10);

	if (ping_interval < INT_MIN || ping_interval > INT_MAX)
		goto out;

	if (!(!eptr || *eptr == '\0'))
		goto out;

	if (ping_interval) {
		xstate->ping_timeout = ping_timeout;
		xstate->ping_interval = ping_interval;
	}

	agh_xmpp_conn_setup(mstate, jid_node, jid_domain, jid_resource, pass, ka_interval, ka_timeout);

	if (!xstate->xmpp_conn) {
		g_print("%s: connection setup failed\n",__FUNCTION__);
		goto out;
	}

	return;

out:
	ka_interval = 0;
	ka_timeout = 0;
	ping_interval = 0;
	ping_timeout = 0;
	uci_unload(xstate->uci_ctx, ptr.p);
	uci_free_context(xstate->uci_ctx);
	xstate->xpackage = NULL;
	xstate->xsection = NULL;
	xstate->uci_ctx = NULL;
out_noctx:
	return;
}

static gboolean xmpp_idle(gpointer data) {
	struct agh_state *mstate = data;
	struct xmpp_state *xstate = mstate->xstate;
	gint xmpp_client_connect_status;
	guint i;
	const gchar *altdomain;
	gint altport;
	const gchar *altport_tmp;
	gchar *eptr;

	xmpp_client_connect_status = 0;
	altdomain = NULL;
	altport = 0;
	eptr = NULL;
	altport_tmp = NULL;

	switch(xstate->xmpp_idle_state) {
	case 0:
		altdomain = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_ALTDOMAIN);

		altport_tmp = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_ALTPORT);

		if (altport_tmp) {
			altport = strtol(altport_tmp, &eptr, 10);

			if (altport < INT_MIN || altport > INT_MAX)
				altport = 0;

			if (!(!eptr || *eptr == '\0'))
				altport = 0;

			eptr = NULL;
		}

		agh_xmpp_config_init(mstate);

		if (!xstate->uci_ctx) {
			g_print("Invalid config!\n");
			break;
		}

		xmpp_client_connect_status = xmpp_connect_client(xstate->xmpp_conn, altdomain, altport, xmpp_connection_handler, mstate);
		if (xmpp_client_connect_status) {
			agh_log_xmpp_dbg("can not connect (code=%" G_GINT16_FORMAT")", xmpp_client_connect_status);
			break;
		}

		xstate->xmpp_idle_state++;
		agh_log_xmpp_dbg("state changed");
		/* fall through */
	case 1:
		/* run strophe event loop, once */
		xmpp_run_once(xstate->xmpp_ctx, AGH_XMPP_RUN_ONCE_INTERVAL);
		agh_xmpp_send_out_messages(mstate);
		break;
	case 2:
		if (!mstate->exiting)
			xstate->xmpp_idle_state = 0;

		break;
	default:
		g_print("%s: unknown state %" G_GUINT16_FORMAT"\n",__FUNCTION__, xstate->xmpp_idle_state);
		xstate->xmpp_idle_state = 0;
	}

	if (mstate->exiting) {
		if (xstate->xmpp_conn) {
			i = xstate->xmpp_idle_state;
			xmpp_disconnect(xstate->xmpp_conn);

			if (xstate->xmpp_idle_state == 1)
				do {
					xmpp_run_once(xstate->xmpp_ctx, AGH_XMPP_RUN_ONCE_INTERVAL);
				} while (i == xstate->xmpp_idle_state);
		}

		xstate->xmpp_idle_state = 2;
		g_print("%s: asked to exit\n",__FUNCTION__);
		mstate->mainloop_needed--;
		xstate->xmpp_evs = NULL;
		return FALSE;

	}

	return TRUE;
}

static void agh_xmpp_start_statemachine(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;

	mstate->mainloop_needed++;
	xstate->xmpp_evs = g_idle_source_new();
	g_source_set_callback(xstate->xmpp_evs, xmpp_idle, mstate, NULL);
	xstate->xmpp_evs_tag = g_source_attach(xstate->xmpp_evs, mstate->ctx);
	g_source_unref(xstate->xmpp_evs);

	return;
}

gint agh_xmpp_init(struct agh_state *mstate) {
	struct xmpp_state *xstate;

	mstate->xstate = g_try_malloc0(sizeof(struct xmpp_state));
	if (!mstate->xstate) {
		g_print("%s: failure while allocating XMPP state\n",__FUNCTION__);
		return -ENOMEM;
	}

	xstate = mstate->xstate;

	xmpp_set_handlers_ext(mstate);

	xstate->outxmpp_messages = g_queue_new();

	g_print("XMPP library init\n");
	xmpp_initialize();

	/* Create XMPP library context */
	agh_xmpp_config_init(mstate);

	if (xstate->uci_ctx) {
		agh_xmpp_start_statemachine(mstate);
	}
	else
		return -EINVAL;

	return 0;
}

gint agh_xmpp_deinit(gpointer data) {
	struct agh_state *mstate = data;
	struct xmpp_state *xstate = mstate->xstate;

	g_print("XMPP deinit.\n");

	if (xstate->ping_interval) {
		xmpp_timed_handler_delete(xstate->xmpp_conn, ping_timeout_handler);
		xmpp_timed_handler_delete(xstate->xmpp_conn, ping_handler);
	}

	if (xstate->xmpp_conn)
		xmpp_conn_release(xstate->xmpp_conn);

	if (xstate->jid) {
		xmpp_free(xstate->xmpp_ctx, xstate->jid);
		xstate->jid = NULL;
	}

	if (xstate->xmpp_ctx)
		xmpp_ctx_free(xstate->xmpp_ctx);

	xmpp_shutdown();
	agh_xmpp_caps_entity_dealloc(xstate->e);
	xstate->e = NULL;
	xstate->xmpp_conn = NULL;
	xstate->xmpp_ctx = NULL;

	xstate->xmpp_evs_tag = 0;

	if (xstate->outxmpp_messages) {
		g_queue_foreach(xstate->outxmpp_messages, discard_xmpp_messages, xstate);
		g_queue_free(xstate->outxmpp_messages);
		xstate->outxmpp_messages = NULL;
	}

	if (xstate->controllers) {
		g_queue_free(xstate->controllers);
		xstate->controllers = NULL;
	}

	xstate->ping_timeout = 0;
	xstate->ping_interval = 0;

	if (xstate->uci_ctx) {
		uci_unload(xstate->uci_ctx, xstate->xpackage);
		uci_free_context(xstate->uci_ctx);
		xstate->uci_ctx = NULL;
		xstate->xpackage = NULL;
		xstate->xsection = NULL;
	}

	g_free(xstate);

	return 0;
}
