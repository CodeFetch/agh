#include <glib.h>
#include "messages.h"
#include "agh.h"
#include "commands.h"

/* Convenience function to allocate a message. Simply calls g_malloc0. */
struct agh_message *msg_alloc(void) {
	return g_malloc0(sizeof(struct agh_message));
}

/*
 * Deallocates a message, and it's CSP, when known.
 * The GmainContext object that's part of a message is not unreferenced.
*/
void msg_dealloc(struct agh_message *m) {
	struct text_csp *csptext;
	struct command *cmd;

	csptext = NULL;
	cmd = NULL;

	if (!m)
		return;

	if (m->csp) {
		switch(m->msg_type) {
		case MSG_INVALID:
			g_print("%s: type %" G_GUINT16_FORMAT" message\n",__FUNCTION__,m->msg_type);
			break;
		case MSG_EXIT:
			break;
		case MSG_RECVTEXT:
		case MSG_SENDTEXT:
			csptext = m->csp;
			//g_print("%s: deallocating text %s\n",__FUNCTION__,csptext->text);
			g_free(csptext->text);
			g_free(csptext);
			csptext = NULL;
			break;
		case MSG_SENDCMD:
		case MSG_EVENT:
			cmd = m->csp;
			cmd_free(cmd);
			break;
		default:
			g_print("%s: unknown CSP type (%" G_GUINT16_FORMAT")\n", __FUNCTION__,m->msg_type);
			break;
		}
	}

	m->csp = NULL;
	m->msg_type = MSG_INVALID;
	m->src = NULL;
	m->dest = NULL;
	g_free(m);
	return;
}

gint msg_send(struct agh_message *m, struct agh_comm *src_comm, struct agh_comm *dest_comm) {

	if (!m)
		return 1;

	if ((!dest_comm) && (!src_comm)) {
		g_print("%s: sender and recipient comms where NULL\n",__FUNCTION__);
		msg_dealloc(m);
		return 1;
	}

	if (!dest_comm)
		dest_comm = src_comm;

	if (dest_comm->teardown_in_progress) {
		g_print("%s: not sending message to %s due to teardown being in progress\n",__FUNCTION__,dest_comm->name);
		msg_dealloc(m);
		return 1;
	}

	m->src = src_comm;
	m->dest = dest_comm;

	//g_print("%s: message %s -> %s\n",__FUNCTION__,m->src->name, m->dest->name);

	g_main_context_invoke(m->dest->ctx, agh_handle_message_inside_dest_thread, m);

	return 0;
}

gboolean agh_handle_message_inside_dest_thread(gpointer data) {
	struct agh_message *m = data;
	guint num_handlers;
	struct handler *h;
	struct agh_message *answer;
	guint i;
	GQueue *handlers;

	num_handlers = 0;
	answer = NULL;
	h = NULL;

	if (!m) {
		g_print("%s: NULL message\n",__FUNCTION__);
		return FALSE;
	}

	handlers = m->dest->handlers;

	if (m->dest->teardown_in_progress) {
		g_print("%s (%s): deallocating message\n",__FUNCTION__,m->dest->name);
		msg_dealloc(m);
		return FALSE;
	}

	if (handlers)
		num_handlers = g_queue_get_length(handlers);
	else {
		g_print("%s: a message has been received, but no handlers queue is allocated\n",__FUNCTION__);
		msg_dealloc(m);
		return FALSE;
	}

	//g_print("%s running in %s\n",__FUNCTION__,m->dest->name);

	for (i=0;i<num_handlers;i++) {
		h = g_queue_peek_nth(handlers, i);
		if (h->enabled) {
			//g_print("handle(%s)\n",h->name);
			answer = h->handle(h, m);
			if (answer) {
				answer->src = m->dest;
				answer->dest = m->src;
				msg_send(answer, answer->src, answer->dest);
			}
			answer = NULL;
		}
	}

	msg_dealloc(m);

	return FALSE;
}

struct agh_comm *agh_comm_setup(GQueue *handlers, GMainContext *ctx, gchar *name) {
	struct agh_comm *comm;

	comm = NULL;

	if (!handlers) {
		g_print("%s: COMM setup for %s failed: handlers GQueue was NULL\n",__FUNCTION__, name ? name : "(unknown)");
		return comm;
	}

	comm = g_malloc0(sizeof(struct agh_comm));

	comm->name = name;
	comm->handlers = handlers;
	comm->ctx = ctx;

	return comm;
}

void agh_comm_teardown(struct agh_comm *comm) {
	guint i;

	i = AGH_MAX_MESSAGEWAIT_ITERATIONS;

	if (!comm)
		return;

	comm->teardown_in_progress = TRUE;

	do {
		if (g_main_context_iteration(comm->ctx, FALSE))
			i = AGH_MAX_MESSAGEWAIT_ITERATIONS;

		i--;
	} while (i);

	comm->name = NULL;
	comm->handlers = NULL;
	comm->ctx = NULL;
	g_free(comm);

	return;
}

void agh_comm_disable(struct agh_comm *comm, gboolean enabled) {
	comm->teardown_in_progress = enabled;
	return;
}
