/* handlers */
#include "handlers.h"
#include <glib.h>
#include "messages.h"

GQueue *handlers_setup(void) {
	GQueue *hq;

	hq = g_queue_new();

	return hq;
}

void handlers_teardown(GQueue *handlers) {
	guint num_handlers;

	if (!handlers) {
		g_print("handlers: NULL handlers queue passed in. No action taken, but something might be going wrong.\n");
		return;
	}

	num_handlers = g_queue_get_length(handlers);
	if (num_handlers) {
		g_print("handlers: %" G_GUINT16_FORMAT" handlers are still registered, this is going to leak memory!\n",num_handlers);
	}
	g_queue_free_full(handlers, g_free);

	return;
}

void handler_register(GQueue *handlers, struct handler *h) {

	if ((!h) || (!handlers)) {
		g_print("handlers: tried to register a NULL handler, or to add an handler to a NULL queue.\n");
	}
	else {
		g_queue_push_tail(handlers, h);
		g_print("handlers: an handler has been registered.\n");
	}
	return;
}

/* Initialize handlers.
 * Each handler's init function is invoked, if the handler is actually enabled and the callback is not NULL.
 * Some handler's structure data fields are initialized in any case.
*/
void handlers_init(GQueue *handlers, gpointer data) {
	guint i;
	guint num_handlers;
	struct handler *h;

	g_print("handlers: init is taking place.\n");
	num_handlers = 0;
	if (!handlers) {
		g_print("handlers: WARNING during init, passed in a NULL queue.\n");
		return;
	}

	num_handlers = g_queue_get_length(handlers);
	for (i=0;i<num_handlers;i++) {
		g_print("*");
		h = g_queue_peek_nth(handlers, i);

		h->handlers_queue = handlers;

		if (data)
			h->handler_data = data;

		if (h->enabled && h->handler_initialize)
			h->handler_initialize(h);
	}

	return;
}

void handlers_finalize_single(gpointer data, gpointer user_data) {
	struct handler *h = data;

	if (h->enabled && h->handler_finalize) {
		g_print("/");
		h->handler_finalize(h);
	}

	/* XXX: a better way to do this? */
	g_queue_remove(h->handlers_queue, h);
	h->handlers_queue = NULL;

	h->handler_data = NULL;

	if (!h->on_stack) {
		g_print("handlers: freeing an handler that has not been allocated in, or declared to be, in the stack. This needs to be looked at.\n");
		g_free(h);
		h = NULL;
	}

	return;
}

void handlers_finalize(GQueue *handlers) {
	g_print("handlers: finalizing handlers.\n");
	g_queue_foreach(handlers, handlers_finalize_single, NULL);
	return;
}
