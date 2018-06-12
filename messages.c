#include "messages.h"
#include "agh.h"

struct agh_message *msg_alloc(gsize len) {
	struct agh_message *m;

	m = NULL;
	m = g_malloc0(sizeof(struct agh_message));

	if (len)
		m->csp = g_malloc0(len);

	m->csp_len = len;

	return m;
}

void msg_dealloc(struct agh_message *m) {
	struct text_csp *csptext;

	if (m->csp_len != 0) {
		switch(m->msg_type) {
		case MSG_RECVTEXT:
		case MSG_SENDTEXT:
			csptext = m->csp;
			g_free(csptext->text);
			csptext->text = NULL;
			g_free(m->csp);
			m->csp = NULL;
			break;
		default:
			g_print("Unknown CSP type detected while deallocating a message; leaking memory.\n");
			break;
		}
	}
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
