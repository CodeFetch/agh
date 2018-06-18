#include "messages.h"
#include "agh.h"
#include "commands.h"

struct agh_message *msg_alloc(void) {
	struct agh_message *m;

	m = g_malloc0(sizeof(struct agh_message));

	return m;
}

void msg_dealloc(struct agh_message *m) {
	struct text_csp *csptext;
	struct command *cmd;

	csptext = NULL;
	cmd = NULL;

	if (m->csp) {
		switch(m->msg_type) {
		case MSG_RECVTEXT:
		case MSG_SENDTEXT:
			csptext = m->csp;
			g_print("Deallocating text %s\n",csptext->text);
			g_free(csptext->text);
			csptext = NULL;
			g_free(m->csp);
			break;
		case MSG_RECVCMD:
		case MSG_SENDCMD:
			g_print("CMD DEALLOC matches.\n");
			cmd = m->csp;
			cmd_free(cmd);
			break;
		default:
			g_print("Unknown CSP type (%" G_GUINT16_FORMAT") detected while deallocating a message; leaking memory.\n", m->msg_type);
			break;
		}
	}

	m->csp = NULL;
	g_free(m);
	return;
}

guint msg_prepare(struct agh_message *m, GAsyncQueue *src_comm, GAsyncQueue *dest_comm) {
	guint result;

	if (!m || !src_comm || !dest_comm) {
		g_print("AGH messages: received an undeliverable message: ");
		if (!m)
			g_print("message was NULL.\n");
		if (!src_comm)
			g_print("source COMM queue was NULL.\n");
		if (!dest_comm)
			g_print("dest COMM queue was NULL.\n");

		result = 1;
	}
	else {
		m->dest_comm = dest_comm;
		m->src_comm = src_comm;
		result = 0;
	}

	return result;
}

void msg_send(struct agh_message *m) {
	GQueue *envelope_queue;

	if (!m->dest_comm) {
		g_print("AGH messages: destination seems not ready. Message is being discarded.\n");
		msg_dealloc(m);
	}
	else {
		envelope_queue = g_queue_new();
		g_queue_push_tail(envelope_queue, m);
		g_async_queue_push(m->dest_comm, envelope_queue);
	}

	return;
}
