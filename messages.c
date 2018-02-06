#include "messages.h"
#include "agh.h"

struct agh_message *msg_alloc(gsize len) {
	struct agh_message *m;

	m = NULL;
	if (len) {
		m = g_malloc0(sizeof(struct agh_message));
		m->csp = g_malloc0(len);
		m->len = len;
	}
	return m;
}

void msg_dealloc(struct agh_message *m) {
	g_free(m->csp);
	m->csp = NULL;
	g_free(m);
	m = NULL;
	return;
}
