#include <glib.h>
#include "agh.h"
#include "xmpp.h"
#include "handlers.h"
#include "xmpp_handlers.h"
#include "messages.h"

gpointer xmpp_thread_start(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	/* Should a memory allocation failure occur, GLib will terminate the application. */
	ct->thread_data = g_malloc0(sizeof(struct xmpp_state));
	xstate = ct->thread_data;

	ct->handlers = handlers_setup();

	/* Handlers are registered from inside the function called here. */
	xmpp_set_handlers_ext(ct);

	/* We can perform messaging setup here, since no sources are called for now; but clearly, things like the outgoing XMPP messages queue (outxmpp_messages) should be initialized and thus ready to use by that time. */
	agh_thread_eventloop_setup(ct, FALSE);
	handlers_init(ct->handlers, ct);

	xstate->xmpp_evs = g_idle_source_new();
	g_source_set_callback(xstate->xmpp_evs, xmpp_idle, ct, NULL);
	xstate->xmpp_evs_tag = g_source_attach(xstate->xmpp_evs, ct->evl_ctx);
	g_source_unref(xstate->xmpp_evs);

	xstate->outxmpp_messages = g_queue_new();

	ct->comm = agh_comm_setup(ct->handlers, ct->evl_ctx, ct->thread_name);

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
	xmpp_conn_set_keepalive(xstate->xmpp_conn, AGH_XMPP_TCP_KEEPALIVE_TIMEOUT, AGH_XMPP_TCP_KEEPALIVE_INTERVAL);

	xmpp_handler_add(xstate->xmpp_conn, version_handler, "jabber:iq:version", "iq", NULL, ct);
	xmpp_handler_add(xstate->xmpp_conn, message_handler, NULL, "message", NULL, ct);
	g_print("%s: entering main loop\n",ct->thread_name);

	g_main_loop_run(ct->evl);

	g_print("XMPP deinit.\n");

	xmpp_conn_release(xstate->xmpp_conn);
	xmpp_ctx_free(xstate->xmpp_ctx);
	xmpp_shutdown();

	/*
	 * Valgrind reported this should not be done. I guess it's because when we return FALSE from the handler, since we already dropped our reference to the source, it is actually deallocated.
	 * That said, when we reach here, the main loop is stopped, but not unreferenced yet. So something isn't clear yet.
	 * Code was:
	 * g_source_destroy(xstate->xmpp_evs);
	*/
	xstate->xmpp_evs_tag = 0;

	agh_comm_teardown(ct->comm);
	agh_thread_eventloop_teardown(ct);

	return data;
}

void xmpp_thread_deinit(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;

	handlers_finalize(ct->handlers);
	handlers_teardown(ct->handlers);
	ct->handlers = NULL;

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

	switch(status) {
	case XMPP_CONN_CONNECT:
		pres = xmpp_presence_new(ctx);
		xmpp_send(xstate->xmpp_conn, pres);
		xmpp_stanza_release(pres);
		pres = NULL;
		g_print("%s (%s): connected\n",ct->thread_name,__FUNCTION__);
		break;
	case XMPP_CONN_DISCONNECT:
		g_print("%s (%s): disconnected\n",ct->thread_name,__FUNCTION__);
		xstate->xmpp_idle_state++;
		break;
	case XMPP_CONN_FAIL:
		g_print("%s (%s): connection failed\n",ct->thread_name,__FUNCTION__);
		xstate->xmpp_idle_state++;
		break;
	default:
		g_print("%s (%s): unknown status\n",ct->thread_name,__FUNCTION__);
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
	xmpp_ctx_t __attribute__((unused)) *ctx;
	xmpp_stanza_t *body;
	const gchar *type;
	gchar *intext;
	struct agh_message *m;
	struct agh_thread *ct;
	struct text_csp *tcsp;

	m = NULL;
	intext = NULL;
	type = NULL;
	body = NULL;
	tcsp = NULL;

	ct = userdata;
	xstate = ct->thread_data;
	ctx = xstate->xmpp_ctx;

	/* body should not be freed */
	body = xmpp_stanza_get_child_by_name(stanza, "body");
	if (!body)
		return 1;

	/* If type is NULL or is an error, stop here; type should not be freed. */
	type = xmpp_stanza_get_type(stanza);
	if (type != NULL && !g_strcmp0(type, "error"))
		return 1;

	intext = xmpp_stanza_get_text(body);

	m = msg_alloc();
	tcsp = g_malloc0(sizeof(struct text_csp));
	m->csp = tcsp;

	tcsp->text = intext;
	m->msg_type = MSG_RECVTEXT;
	if (msg_send(m, ct->comm, ct->agh_comm)) {
		g_print("%s: unable to send received XMPP message to core\n",__FUNCTION__);
	}

	/*
	 * Given the current situation, we can avoid the
	 * xmpp_free(ctx, intext);
	 * that was here previously. This was true with libstrophe 0.9.2.
	*/

	return 1;
}

gboolean xmpp_idle(gpointer data) {
	struct agh_thread *ct = data;
	struct xmpp_state *xstate = ct->thread_data;
	gint xmpp_client_connect_status;
	guint i;

	xmpp_client_connect_status = 0;

	switch(xstate->xmpp_idle_state) {
	case 0:
		xmpp_client_connect_status = xmpp_connect_client(xstate->xmpp_conn, NULL, 0, xmpp_connection_handler, ct);
		if (xmpp_client_connect_status)
			break;

		xstate->xmpp_idle_state++;
		/* fall through */
	case 1:
		agh_xmpp_send_out_messages(xstate);
		/* run strophe event loop, once */
		xmpp_run_once(xstate->xmpp_ctx, AGH_XMPP_RUN_ONCE_INTERVAL);
		break;
	case 2:
		if (!xstate->exit) {
			xstate->xmpp_idle_state = 0;
		}
		break;
	default:
		g_print("%s: unknown state %" G_GUINT16_FORMAT"\n",__FUNCTION__, xstate->xmpp_idle_state);
		xstate->xmpp_idle_state = 0;
	}

	if (xstate->exit) {
		i = xstate->xmpp_idle_state;
		xmpp_disconnect(xstate->xmpp_conn);

		do {
			xmpp_run_once(xstate->xmpp_ctx, AGH_XMPP_RUN_ONCE_INTERVAL);
		} while (i == xstate->xmpp_idle_state);

		xstate->xmpp_idle_state = 2;
		g_main_loop_quit(ct->evl);
		g_print("%s: asked to exit\n",ct->thread_name);
		return FALSE;

	}

	return TRUE;
}

void agh_xmpp_send_out_messages(gpointer data) {
	struct xmpp_state *xstate = data;
	guint num_messages;
	xmpp_stanza_t *reply;
	xmpp_ctx_t *ctx;
	gchar *text;
	gchar *id;

	id = NULL;
	text = NULL;
	reply = NULL;

	ctx = xstate->xmpp_ctx;
	num_messages = g_queue_get_length(xstate->outxmpp_messages);

	if (num_messages) {

		if (num_messages >= AGH_XMPP_MAX_OUTGOING_QUEUED_MESSAGES) {
			g_print("XMPP: maximum number of messages queued for sending has been received; discarding all of them.\n");
			g_queue_foreach(xstate->outxmpp_messages, discard_xmpp_messages, xstate);
			return;
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

	/* Should I use "text" here? */
	g_free(data);

	return;
}

void xmpp_set_handlers_ext(struct agh_thread *ct) {
	struct handler *xmpp_sendmsg_handler;
	struct handler *xmpp_cmd_handler;
	struct handler *xmpp_event_handler;
	struct handler *xmpp_exit_handler;

	xmpp_sendmsg_handler = NULL;
	xmpp_cmd_handler = NULL;
	xmpp_event_handler = NULL;
	xmpp_exit_handler = NULL;

	xmpp_sendmsg_handler = handler_new("xmpp_sendmsg_handler");
	handler_set_handle(xmpp_sendmsg_handler, xmpp_sendmsg_handle);
	handler_enable(xmpp_sendmsg_handler, TRUE);

	xmpp_cmd_handler = handler_new("xmpp_cmd_handler");
	handler_set_handle(xmpp_cmd_handler, xmpp_cmd_handle);
	handler_enable(xmpp_cmd_handler, TRUE);

	xmpp_event_handler = handler_new("xmpp_event_handler");
	handler_set_handle(xmpp_event_handler,xmpp_event_handle);
	handler_enable(xmpp_event_handler, FALSE);

	xmpp_exit_handler = handler_new("xmpp_exit_handler");
	handler_set_handle(xmpp_exit_handler, xmpp_exit_handle);
	handler_enable(xmpp_exit_handler, TRUE);

	handler_register(ct->handlers, xmpp_sendmsg_handler);
	handler_register(ct->handlers, xmpp_cmd_handler);
	handler_register(ct->handlers, xmpp_event_handler);
	handler_register(ct->handlers, xmpp_exit_handler);

	return;
}
