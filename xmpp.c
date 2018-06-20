#include <glib.h>
#include "agh.h"
#include "xmpp.h"
#include "aghservices.h"
#include "handlers.h"
#include "xmpp_handlers.h"

void xmpp_thread_init(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate;

	/* Should a memory allocation failure occur, GLib will terminate the application. */
	ct->thread_data = g_malloc0(sizeof(struct xmpp_state));
	xstate = ct->thread_data;

	ct->handlers = handlers_setup();

	/* Handlers are registered from inside the function called here. */
	xmpp_set_handlers_ext(ct);

	/* We can perform messaging setup here, since no sources are called for now; but clearly, things like the outgoing XMPP messages queue (outxmpp_messages) should be initialized and thus ready to use by that time. */
	aghservices_messaging_setup(ct, FALSE);
	handlers_init(ct->handlers, ct);

	g_print("XMPP library init\n");
	xmpp_initialize();

	/* Create XMPP library context */
	xstate->xmpp_log = xmpp_get_default_logger(XMPP_LEVEL_INFO);

	/* First parameter is NULL since we don't provide our own memory allocator. */
	xstate->xmpp_ctx = xmpp_ctx_new(NULL, xstate->xmpp_log);

	xstate->xmpp_conn = xmpp_conn_new(xstate->xmpp_ctx);

	xmpp_conn_set_jid(xstate->xmpp_conn, "mrkiko@jabber.linux.it");
	xmpp_conn_set_pass(xstate->xmpp_conn, "dviselect_123_456");
	xmpp_conn_set_flags(xstate->xmpp_conn, XMPP_CONN_FLAG_MANDATORY_TLS);

	xstate->xmpp_evs = g_idle_source_new();
	g_source_set_callback(xstate->xmpp_evs, xmpp_idle, ct, NULL);
	xstate->xmpp_evs_tag = g_source_attach(xstate->xmpp_evs, ct->evl_ctx);

	xstate->outxmpp_messages = g_queue_new();

	return;
}

gpointer xmpp_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	xmpp_connect_client(xstate->xmpp_conn, NULL, 0, xmpp_connection_handler, ct);

	g_main_loop_run(ct->evl);

	return data;
}

void xmpp_thread_deinit(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;
	guint num_undelivered_messages;

	g_print("XMPP deinit.\n");

	xmpp_conn_release(xstate->xmpp_conn);
	xmpp_ctx_free(xstate->xmpp_ctx);
	xmpp_shutdown();
	handlers_finalize(ct->handlers);
	handlers_teardown(ct->handlers);
	ct->handlers = NULL;

	num_undelivered_messages = g_queue_get_length(xstate->outxmpp_messages);
	if (num_undelivered_messages) {
		g_print("XMPP handler: losing %" G_GUINT16_FORMAT" pending messages. This should not happen.\n",num_undelivered_messages);
	}
	g_queue_free_full(xstate->outxmpp_messages, g_free);
	xstate->outxmpp_messages = NULL;
	g_free(ct->thread_data);
	xstate = NULL;
	return;
}

void xmpp_connection_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata) {
	struct xmpp_state *xstate;
	xmpp_stanza_t *pres;
	xmpp_ctx_t *ctx;
	struct agh_thread *ct;

	ct = userdata;
	xstate = ct->thread_data;

	ctx = xstate->xmpp_ctx;
	pres = NULL;
	xstate->status = status;

	if (status == XMPP_CONN_CONNECT) {
		xmpp_handler_add(conn, version_handler, "jabber:iq:version", "iq", NULL, ct);
		xmpp_handler_add(conn, message_handler, NULL, "message", NULL, ct);
		pres = xmpp_presence_new(ctx);
		xmpp_send(xstate->xmpp_conn, pres);
		xmpp_stanza_release(pres);
		pres = NULL;
	} else {
		g_print("We are disconnected from XMPP.\n");
		xmpp_stop(ctx);
	}

	return;
}

int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
	xmpp_stanza_t *reply, *query, *name, *version, *text;
	const char *ns;
	struct agh_thread *ct;
	struct xmpp_state *xstate;
	xmpp_ctx_t *ctx;

	ct = userdata;
	xstate = ct->thread_data;
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
	xmpp_stanza_set_text(text, "AGH");
	xmpp_stanza_add_child(name, text);
	xmpp_stanza_release(text);

	version = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(version, "version");
	xmpp_stanza_add_child(query, version);
	xmpp_stanza_release(version);

	text = xmpp_stanza_new(ctx);
	xmpp_stanza_set_text(text, "0.01");
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
	const char *type;
	gchar *intext;
	struct agh_message *m;
	struct agh_thread *ct;
	struct text_csp *tcsp;

	ct = userdata;
	xstate = ct->thread_data;
	ctx = xstate->xmpp_ctx;
	tcsp = g_malloc0(sizeof(struct text_csp));

	body = xmpp_stanza_get_child_by_name(stanza, "body");
	if (!body)
		return 1;

	type = xmpp_stanza_get_type(stanza);
	if (type != NULL && g_strcmp0(type, "error") == 0)
		return 1;

	intext = xmpp_stanza_get_text(body);

	m = msg_alloc();
	msg_prepare(m, ct->comm, ct->agh_comm);
	m->csp = tcsp;

	tcsp->text = g_strdup(intext);
	m->msg_type = MSG_RECVTEXT;
	msg_send(m);

	xmpp_free(ctx, intext);

	return 1;
}

gboolean xmpp_idle(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	if (xstate->status == XMPP_CONN_CONNECT)
		xmpp_send_out_messages(xstate);

	xmpp_run_once(xstate->xmpp_ctx, 180);
	return TRUE;
}

void xmpp_send_out_messages(gpointer data) {
	struct xmpp_state *xstate = data;
	guint num_messages;
	xmpp_stanza_t *reply;
	xmpp_ctx_t *ctx;
	gchar *text;
	gchar *id = NULL;

	ctx = xstate->xmpp_ctx;
	num_messages = g_queue_get_length(xstate->outxmpp_messages);
	if (num_messages) {

		if (num_messages >= MAX_XMPP_QUEUED_MESSAGES) {
			g_print("XMPP: maximum number of messages queued for sending has been received; discarding all of them.\n");
			g_queue_foreach(xstate->outxmpp_messages, discard_xmpp_messages, xstate);
		}

		if (xstate->msg_id == G_MAXUINT64)
			xstate->msg_id = 0;

		id = g_strdup_printf("AGH_%" G_GUINT64_FORMAT"",xstate->msg_id);
		reply = xmpp_message_new(ctx, "chat", "mrkiko@alpha-labs.net", id);
		text = g_queue_pop_head(xstate->outxmpp_messages);
		xmpp_message_set_body(reply, text);
		xmpp_send(xstate->xmpp_conn, reply);
		xmpp_stanza_release(reply);
		g_free(text);
		g_free(id);
		xstate->msg_id++;
	}
	return;
}

void discard_xmpp_messages(gpointer data, gpointer userdata) {
	struct xmpp_state *xstate = userdata;
	gchar *xmpp_message_text = data;

	g_print("Discarding element: %s\n", xmpp_message_text);
	g_queue_remove(xstate->outxmpp_messages, data);
	g_free(data);

	return;
}

void xmpp_set_handlers_ext(struct agh_thread *ct) {
	struct handler *xmpp_sendmsg_handler;
	struct handler *xmpp_cmd_handler;
	struct handler *xmpp_event_handler;

	xmpp_sendmsg_handler = NULL;
	xmpp_cmd_handler = NULL;

	xmpp_sendmsg_handler = handler_new("xmpp_sendmsg_handler");
	handler_set_handle(xmpp_sendmsg_handler, xmpp_sendmsg_handle);
	handler_enable(xmpp_sendmsg_handler, TRUE);

	xmpp_cmd_handler = handler_new("xmpp_cmd_handler");
	handler_set_handle(xmpp_cmd_handler, xmpp_cmd_handle);
	handler_enable(xmpp_cmd_handler, TRUE);

	xmpp_event_handler = handler_new("xmpp_event_handler");
	handler_set_handle(xmpp_event_handler,xmpp_event_handle);
	handler_enable(xmpp_event_handler, TRUE);

	handler_register(ct->handlers, xmpp_sendmsg_handler);
	handler_register(ct->handlers, xmpp_cmd_handler);
	handler_register(ct->handlers, xmpp_event_handler);

	return;
}
