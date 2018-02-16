/* handlers */
#include "handlers.h"
#include <glib.h>
#include "messages.h"

GQueue *handlers_setup(void) {
	g_print("handlers: allocating queue.\n");
	return g_queue_new();
}

void handlers_teardown(GQueue *handlers) {
	g_print("handlers: teardown in progress.\n");
	guint num_handlers;

	if (!handlers) {
		g_print("handlers: NULL handlers queue passed in. No action taken, but something might be going wrong.\n");
		return;
	}

	num_handlers = g_queue_get_length(handlers);
	if (num_handlers) {
		g_print("handlers: %d handlers are still registered, this is going to leak memory!\n",num_handlers);
	}
	g_queue_free(handlers);
	handlers = NULL;
	return;
}

void handler_register(GQueue *handlers, struct handler *h) {

	if ((!h) || (!handlers)) {
		g_print("handlers: tried to register a NULL handler, or to add an handler to a NULL queue.\n");
	}
	else {
		g_queue_push_head(handlers, h);
		g_print("handlers: an handler has been registered.\n");
	}
	return;
}

void handlers_init(GQueue *handlers) {
	g_print("handlers: init is taking place.\n");
	g_queue_foreach(handlers, handlers_init_single, NULL);
	return;
}

void handlers_init_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	g_print("*");
	if (h->enabled)
		h->handler_initialize(h);
	return;
}

void handlers_finalize_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	g_print("X");
	h->handler_finalize(h);

	/* XXX: a better way to do this? */
	g_queue_remove(h->handlers_queue, h);
	if (!h->on_stack) {
		g_print("handlers: freeing an handler that has not been allocated in, or declared to be, in the stack. Freeing it now, but this needs to be looked at.\n");
		g_free(h);
	}
	return;
}

void handlers_finalize(GQueue *handlers) {
	g_print("handlers: finalizing handlers.\n");
	g_queue_foreach(handlers, handlers_finalize_single, NULL);
	return;
}

struct agh_message *handlers_dispatch_single(gpointer data, gpointer user_data) {
	struct handler *h = data;
	struct agh_message *m = user_data;
	struct agh_message *answer;

	answer = h->handle(data, user_data);

	return answer;
}
