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

void handlers_init(GQueue *handlers, gpointer hsd) {
	guint i;
	guint num_handlers;
	struct handler *h;

	g_print("handlers: init is taking place.\n");
	num_handlers = 0;
	if (!handlers) {
		g_print("WARNING: passed in a NULL queue.\n");
	}
	else {
		num_handlers = g_queue_get_length(handlers);
		for (i=0;i<num_handlers;i++) {
			g_print("*");
			h = g_queue_peek_nth(handlers, i);

			h->handlers_queue = handlers;
			if (h->enabled)
				h->handler_initialize(h, hsd);
		}
	}

	return;
}

void handlers_finalize_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	g_print("X");
	h->handler_finalize(h, user_data);

	/* XXX: a better way to do this? */
	g_queue_remove(h->handlers_queue, h);
	h->handlers_queue = NULL;
	if (!h->on_stack) {
		g_print("handlers: freeing an handler that has not been allocated in, or declared to be, in the stack. Freeing it now, but this needs to be looked at.\n");
		g_free(h);
	}

	return;
}

void handlers_finalize(GQueue *handlers, gpointer hsd) {
	g_print("handlers: finalizing handlers.\n");
	g_queue_foreach(handlers, handlers_finalize_single, hsd);
	return;
}
