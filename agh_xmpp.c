#include <glib.h>
#include <uci.h>
#include "agh_xmpp.h"
#include "agh.h"
#include "agh_handlers.h"
#include "agh_xmpp_handlers.h"
#include "agh_messages.h"
#include "agh_xmpp_caps.h"
#include "agh_commands.h"

gpointer agh_xmpp_init(gpointer data) {
	struct agh_state *mstate = data;
	struct xmpp_state *xstate;

	/* Should a memory allocation failure occur, GLib will terminate the application. */
	mstate->xstate = g_malloc0(sizeof(struct xmpp_state));

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

	return data;
}

void agh_xmpp_deinit(gpointer data) {
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

	g_queue_foreach(xstate->outxmpp_messages, discard_xmpp_messages, xstate);
	g_queue_free(xstate->outxmpp_messages);
	xstate->outxmpp_messages = NULL;
	g_queue_free(xstate->controllers);
	xstate->controllers = NULL;
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

	return;
}

void xmpp_connection_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata) {
	struct xmpp_state *xstate;
	xmpp_stanza_t *pres;
	xmpp_ctx_t *ctx;
	struct agh_state *mstate;

	mstate = userdata;
	xstate = mstate->xstate;

	ctx = xstate->xmpp_ctx;
	pres = NULL;

	switch(status) {
	case XMPP_CONN_CONNECT:
		pres = xmpp_presence_new(ctx);
		agh_xmpp_prepare_entity(xstate);
		agh_xmpp_caps_add_hash(ctx, xstate->e, pres);
		agh_xmpp_caps_entity_dealloc(xstate->e);
		xstate->e = NULL;
		xmpp_send(xstate->xmpp_conn, pres);
		xmpp_stanza_release(pres);
		pres = NULL;
		break;
	case XMPP_CONN_DISCONNECT:
		xstate->xmpp_idle_state++;
		break;
	case XMPP_CONN_FAIL:
		g_print("%s: connection failed\n",__FUNCTION__);
		xstate->xmpp_idle_state++;
		break;
	default:
		g_print("%s: unknown status\n",__FUNCTION__);
	}

	return;
}

int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	xmpp_stanza_t *reply, *query, *name, *version, *text;
	const char *ns;
	struct agh_state *mstate;
	struct xmpp_state *xstate;
	xmpp_ctx_t *ctx;

	mstate = userdata;
	xstate = mstate->xstate;
	ctx = xstate->xmpp_ctx;

	g_print("Received version request from %s\n", xmpp_stanza_get_from(stanza));
	reply = xmpp_stanza_reply(stanza);
	xmpp_stanza_set_type(reply, "result");

	query = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(query, "query");
	ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));

	if (ns) {
		xmpp_stanza_set_ns(query, ns);
	}

	name = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(name, "name");
	xmpp_stanza_add_child(query, name);
	xmpp_stanza_release(name);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, "AGH ("AGH_RELEASE_NAME")");
	xmpp_stanza_add_child(name, text);
	xmpp_stanza_release(text);

	version = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(version, "version");
	xmpp_stanza_add_child(query, version);
	xmpp_stanza_release(version);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, AGH_VERSION);
	xmpp_stanza_add_child(version, text);
	xmpp_stanza_release(text);

	xmpp_stanza_add_child(reply, query);
	xmpp_stanza_release(query);
	xmpp_send(conn, reply);
	xmpp_stanza_release(reply);

	return 1;
}

int message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
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

	m = NULL;
	intext = NULL;
	type = NULL;
	body = NULL;
	receipt_request = NULL;
	from = NULL;
	msg_id = NULL;
	receipt_message = NULL;
	receipt_response = NULL;
	from_barejid = NULL;
	is_a_controller = FALSE;
	i = 0;
	controllers_queue_len = 0;
	current_controller = NULL;
	receipt_response_id = NULL;

	/* body should not be freed */
	body = xmpp_stanza_get_child_by_name(stanza, "body");
	if (!body)
		return 1;

	/* If type is NULL or is an error, stop here; type should not be freed. */
	type = xmpp_stanza_get_type(stanza);
	if (type != NULL && !g_strcmp0(type, "error"))
		return 1;

	from = xmpp_stanza_get_from(stanza);
	if (!from)
		return 1;

	msg_id = xmpp_stanza_get_id(stanza);
	if (!msg_id)
		return 1;

	from_barejid = xmpp_jid_bare(ctx, from);
	controllers_queue_len = g_queue_get_length(xstate->controllers);
	for (i = 0; i<controllers_queue_len;i++) {
		current_controller = g_queue_peek_nth(xstate->controllers, i);
		if (!g_strcmp0(from_barejid, current_controller)) {
			is_a_controller = TRUE;
			break;
		}
	}
	if (!is_a_controller) {
		xmpp_free(ctx, from_barejid);
		from_barejid = NULL;
		return 1;
	}

	intext = xmpp_stanza_get_text(body);

	receipt_request = xmpp_stanza_get_child_by_ns(stanza, AGH_XMPP_STANZA_NS_RECEIPTS);

	if (receipt_request) {
		g_print("%s: receipt request received.\n",__FUNCTION__);

		if (xstate->msg_id == G_MAXUINT64)
			xstate->msg_id = 0;

		receipt_response_id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);
		receipt_message = xmpp_message_new(ctx, NULL, from, receipt_response_id);

		receipt_response = xmpp_stanza_new(ctx);
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
		receipt_response_id = NULL;
	}

	to = xmpp_stanza_get_to(stanza);

	m = agh_xmpp_new_message(from, to, xmpp_stanza_get_id(stanza), intext);

	if (msg_send(m, mstate->comm, NULL)) {
		g_print("%s: unable to send received XMPP message to core\n",__FUNCTION__);
	}

	/*
	 * Given the current situation, we can avoid the
	 * xmpp_free(ctx, intext);
	 * that was here previously. This was true with libstrophe 0.9.2.
	*/

	xmpp_free(ctx, from_barejid);

	return 1;
}

gboolean xmpp_idle(gpointer data) {
	struct agh_state *mstate = data;
	struct xmpp_state *xstate = mstate->xstate;
	gint xmpp_client_connect_status;
	guint i;
	const gchar *altdomain;
	gint altport;
	const gchar *altport_tmp;
	gchar *eptr;
	const gchar *stress_mode;

	xmpp_client_connect_status = 0;
	altdomain = NULL;
	altport = 0;
	eptr = NULL;
	altport_tmp = NULL;
	stress_mode = NULL;

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

		stress_mode = agh_xmpp_getoption(xstate, AGH_XMPP_UCI_OPTION_STRESS_MODE);
		if (stress_mode && !g_strcmp0(stress_mode, AGH_XMPP_UCI_OPTION_STRESS_MODE_ACTIVATE_KEYWORD)) {
			agh_xmpp_start_stressing(mstate);
		}

		agh_xmpp_config_init(mstate);

		xmpp_client_connect_status = xmpp_connect_client(xstate->xmpp_conn, altdomain, altport, xmpp_connection_handler, mstate);
		if (xmpp_client_connect_status)
			break;

		xstate->xmpp_idle_state++;
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
		i = xstate->xmpp_idle_state;
		xmpp_disconnect(xstate->xmpp_conn);

		if (xstate->xmpp_idle_state == 1)
			do {
				xmpp_run_once(xstate->xmpp_ctx, AGH_XMPP_RUN_ONCE_INTERVAL);
			} while (i == xstate->xmpp_idle_state);

		xstate->xmpp_idle_state = 2;
		g_print("%s: asked to exit\n",__FUNCTION__);
		mstate->mainloop_needed--;
		xstate->xmpp_evs = NULL;
		return FALSE;

	}

	return TRUE;
}

void agh_xmpp_send_out_messages(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;
	struct text_csp *tcsp;
	struct agh_message *artificial_message;
	gchar *agh_message_source_name;
	gchar *agh_message_source_from;
	guint i;
	guint controllers_queue_len;
	gchar *current_controller;

	tcsp = NULL;
	artificial_message = NULL;
	agh_message_source_from = NULL;
	agh_message_source_name = NULL;
	i = 0;
	controllers_queue_len = 0;
	current_controller = NULL;

	if (!xstate->outxmpp_messages)
		return;

	//g_print("%s: currently %" G_GUINT16_FORMAT" messages on queue\n",__FUNCTION__, g_queue_get_length(xstate->outxmpp_messages));

	if (!g_queue_get_length(xstate->outxmpp_messages))
		return;

	/* Get a message */
	artificial_message = g_queue_pop_head(xstate->outxmpp_messages);

	/* We expect only MSG_SENDTEXT messages, or in any case, messages with a text_csp. */
	tcsp = artificial_message->csp;

	if (tcsp->source_id) {
		agh_message_source(tcsp->source_id, &agh_message_source_name, &agh_message_source_from);

		if (agh_message_source_from) {
			if (!g_strcmp0(agh_message_source_name, "XMPP"))
				agh_xmpp_send_message(mstate, agh_message_source_from, tcsp->text);
			g_free(agh_message_source_from);
			g_free(agh_message_source_name);
			agh_message_source_from = NULL;
			agh_message_source_name = NULL;
		}
	}
	else {
		//g_print("Broadcast this message.\n");
		controllers_queue_len = g_queue_get_length(xstate->controllers);
		for (i=0;i<controllers_queue_len;i++) {
			current_controller = g_queue_peek_nth(xstate->controllers, i);
			agh_xmpp_send_message(mstate, current_controller, tcsp->text);
		}
	}

	msg_dealloc(artificial_message);

	return;
}

void agh_xmpp_send_message(struct agh_state *mstate, const gchar *to, const gchar *text) {
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_ctx_t *ctx = xstate->xmpp_ctx;

	xmpp_stanza_t *reply;
	gchar *id;
	const gchar *from;
	gchar *local_text;

	id = NULL;
	reply = NULL;
	from = NULL;
	local_text = NULL;

	from = xmpp_conn_get_bound_jid(xstate->xmpp_conn);
	if ((xstate->xmpp_idle_state != 1) || !from || !to || !text)
		return;

	if (xstate->msg_id == G_MAXUINT64)
		xstate->msg_id = 0;

	id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);

	reply = xmpp_message_new(ctx, "chat", to, id);
	local_text = g_strdup(text);
	xmpp_message_set_body(reply, local_text);
	xmpp_stanza_set_from(reply, from);
	xmpp_send(xstate->xmpp_conn, reply);
	xmpp_stanza_release(reply);
	g_free(local_text);
	g_free(id);
	xstate->msg_id++;

	return;
}

void discard_xmpp_messages(gpointer data, gpointer userdata) {
	struct xmpp_state *xstate = userdata;
	struct agh_message *artificial_message = data;
	struct text_csp *tcsp;

	tcsp = artificial_message->csp;

	g_print("[%s]: %s\n",tcsp->source_id ? tcsp->source_id : "unknown source", tcsp->text ? tcsp->text : "unknown text?");

	g_queue_remove(xstate->outxmpp_messages, artificial_message);

	msg_dealloc(artificial_message);

	return;
}

void xmpp_set_handlers_ext(struct agh_state *mstate) {
	struct handler *xmpp_sendmsg_handler;
	struct handler *xmpp_cmd_handler;

	xmpp_sendmsg_handler = NULL;
	xmpp_cmd_handler = NULL;

	xmpp_sendmsg_handler = handler_new("xmpp_sendmsg_handler");
	handler_set_handle(xmpp_sendmsg_handler, xmpp_sendmsg_handle);
	handler_enable(xmpp_sendmsg_handler, TRUE);

	xmpp_cmd_handler = handler_new("xmpp_cmd_handler");
	handler_set_handle(xmpp_cmd_handler, xmpp_cmd_handle);
	handler_enable(xmpp_cmd_handler, TRUE);

	handler_register(mstate->agh_handlers, xmpp_sendmsg_handler);
	handler_register(mstate->agh_handlers, xmpp_cmd_handler);

	return;
}

void agh_xmpp_prepare_entity(struct xmpp_state *xstate) {
	gint id;

	id = 0;

	if (xstate->e)
		return;

	xstate->e = agh_xmpp_caps_entity_alloc();

	id = agh_xmpp_caps_add_entity(xstate->e);

	agh_xmpp_caps_set_entity_data(xstate->e, id, "AGH "AGH_VERSION" ("AGH_RELEASE_NAME")", "client", "bot", NULL);

	agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_FEATURE_RECEIPTS);
	agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_STANZA_NS_CAPS);
	agh_xmpp_caps_add_feature(xstate->e, AGH_XMPP_STANZA_NS_PING);

	return;
}

int discoinfo_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_ctx_t *ctx = xstate->xmpp_ctx;

	const gchar *from;
	const gchar *srcnode;
	xmpp_stanza_t *incoming_query;
	xmpp_stanza_t *response;
	xmpp_stanza_t *capsdata;

	from = NULL;
	srcnode = NULL;
	incoming_query = NULL;
	response = NULL;
	capsdata = NULL;

	incoming_query = xmpp_stanza_get_child_by_name(stanza, AGH_XMPP_STANZA_NAME_QUERY);

	from = xmpp_stanza_get_from(stanza);

	if (!from)
		return 1;

	response = xmpp_iq_new(ctx, AGH_XMPP_STANZA_TYPE_RESULT, xmpp_stanza_get_id(stanza));
	xmpp_stanza_set_to(response, from);
	srcnode = xmpp_stanza_get_attribute(incoming_query, AGH_XMPP_STANZA_ATTR_NODE);

	agh_xmpp_prepare_entity(xstate);
	capsdata = agh_xmpp_caps_get_capsdata(xstate);
	agh_xmpp_caps_entity_dealloc(xstate->e);
	xstate->e = NULL;

	if (srcnode)
		xmpp_stanza_set_attribute(capsdata, AGH_XMPP_STANZA_ATTR_NODE, srcnode);

	xmpp_stanza_add_child(response, capsdata);
	xmpp_stanza_release(capsdata);

	xmpp_stanza_set_from(response, xmpp_conn_get_bound_jid(conn));

	xmpp_send(conn, response);
	xmpp_stanza_release(response);

	return 1;
}

void agh_xmpp_config_init(struct agh_state *mstate) {
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

	package = NULL;
	section = NULL;
	optval = NULL;
	jid_node = NULL;
	jid_resource = NULL;
	jid_domain = NULL;
	pass = NULL;
	ka_interval = 0;
	ka_timeout = 0;
	eptr = NULL;
	ping_timeout = 0;
	ping_interval = 0;
	controllers = NULL;

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

void agh_xmpp_start_statemachine(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;

	mstate->mainloop_needed++;
	xstate->xmpp_evs = g_idle_source_new();
	g_source_set_callback(xstate->xmpp_evs, xmpp_idle, mstate, NULL);
	xstate->xmpp_evs_tag = g_source_attach(xstate->xmpp_evs, mstate->ctx);
	g_source_unref(xstate->xmpp_evs);

	return;
}

const gchar *agh_xmpp_getoption(struct xmpp_state *xstate, gchar *name) {
	gchar *content;
	struct uci_option *option;

	content = NULL;
	option = NULL;

	if (!xstate->xsection)
		return content;

	option = uci_lookup_option(xstate->uci_ctx, xstate->xsection, name);

	if ((!option) || (option->type != UCI_TYPE_STRING))
		return content;

	content = option->v.string;

	return content;
}

GQueue *agh_xmpp_getoption_list(struct xmpp_state *xstate, gchar *name) {
	struct uci_option *option;
	GQueue *res;
	struct uci_element *e;

	option = NULL;
	res = NULL;
	e = NULL;

	if (!xstate->xsection)
		return res;

	option = uci_lookup_option(xstate->uci_ctx, xstate->xsection, name);

	if ((!option) || (option->type != UCI_TYPE_LIST))
		return res;

	res = g_queue_new();
	uci_foreach_element(&option->v.list, e) {
		g_queue_push_tail(res, e->name);
	}

	return res;
}

void agh_xmpp_conn_setup(struct agh_state *mstate, const gchar *node, const gchar *domain, const gchar *resource, const gchar *pass, gint ka_interval, gint ka_timeout) {
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
		g_free(jid);
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

int pong_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	xmpp_stanza_t *iq_ping;

	iq_ping = NULL;

	g_print("%s: nice, I was invoked.\n",__FUNCTION__);

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

struct agh_message *agh_xmpp_new_message(const gchar *from, const gchar *to, const gchar *id, gchar *text) {
	struct agh_message *m;
	struct xmpp_csp *xcsp;

	m = NULL;
	xcsp = NULL;

	if (!from)
		return NULL;

	if ((!id) || (!text))
		return NULL;

	m = msg_alloc();
	xcsp = g_malloc0(sizeof(struct xmpp_csp));
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

xmpp_stanza_t *agh_xmpp_build_ping_base(struct agh_state *mstate, const gchar *to, const gchar *id, gchar *stanza_type) {
	struct xmpp_state *xstate =mstate->xstate;
	xmpp_stanza_t *iq_ping;
	xmpp_stanza_t *ping;
	gchar *msg_id;

	msg_id = NULL;
	iq_ping = NULL;
	ping = NULL;

	if (!stanza_type)
		return NULL;

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

	if (g_strcmp0(stanza_type, AGH_XMPP_STANZA_TYPE_RESULT)) {
		ping = xmpp_stanza_new(xstate->xmpp_ctx);
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

int ping_handler(xmpp_conn_t *const conn, void *const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;
	xmpp_stanza_t *iq_ping;
	gchar *domain;

	iq_ping = NULL;
	domain = NULL;

	if (xstate->xmpp_idle_state != 1)
		return 1;

	domain = xmpp_jid_domain(xstate->xmpp_ctx, xmpp_conn_get_jid(xstate->xmpp_conn));
	iq_ping = agh_xmpp_build_ping_base(mstate, domain, NULL, AGH_XMPP_STANZA_TYPE_GET);
	xmpp_timed_handler_add(xstate->xmpp_conn, ping_timeout_handler, xstate->ping_timeout * 1000, mstate);
	xmpp_send(xstate->xmpp_conn, iq_ping);
	xmpp_stanza_release(iq_ping);
	xmpp_free(xstate->xmpp_ctx, domain);
	iq_ping = NULL;
	domain = NULL;
	xstate->ping_is_timeout = TRUE;

	return 0;
}

int ping_timeout_handler(xmpp_conn_t *const conn, void *const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	if (xstate->xmpp_idle_state != 1)
		return 1;

	g_print("%s: oops!\n",__FUNCTION__);

	xmpp_disconnect(xstate->xmpp_conn);

	return 0;
}

int iq_result_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	struct agh_state *mstate = userdata;
	struct xmpp_state *xstate = mstate->xstate;

	if (xstate->ping_interval && xstate->ping_is_timeout) {
		//g_print("%s: \"resetting\" ping logic\n",__FUNCTION__);
		xmpp_timed_handler_delete(xstate->xmpp_conn, ping_timeout_handler);
		xmpp_timed_handler_add(xstate->xmpp_conn, ping_handler, xstate->ping_interval * 1000, mstate);
		xstate->ping_is_timeout = FALSE;
	}

	return 1;
}

void agh_xmpp_start_stressing(struct agh_state *mstate) {
	struct xmpp_state *xstate = mstate->xstate;

	if (xstate->stress_source)
		return;

	xstate->stress_source = g_timeout_source_new(400);
	g_source_set_callback(xstate->stress_source, agh_xmpp_stressing_callback, mstate, NULL);
	xstate->stress_tag = g_source_attach(xstate->stress_source, mstate->ctx);
	g_source_unref(xstate->stress_source);

	return;
}

gboolean agh_xmpp_stressing_callback(gpointer data) {
	struct agh_state *mstate = data;
	struct xmpp_state *xstate = mstate->xstate;
	struct command *event;

	event = NULL;

	if (mstate->exiting) {
		xstate->stress_source = NULL;
		xstate->stress_tag = 0;
		return FALSE;
	}

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, "STRESS");
	cmd_emit_event(mstate->comm, event);
/*
	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, "STRESS");
	cmd_emit_event(mstate->comm, event);

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, "STRESS");
	cmd_emit_event(mstate->comm, event);

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, "STRESS");
	cmd_emit_event(mstate->comm, event);
*/

	return TRUE;
}
