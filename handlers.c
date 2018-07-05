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

	num_handlers = 0;
	if (!handlers) {
		g_print("handlers: WARNING during init, passed in a NULL queue.\n");
		return;
	}

	num_handlers = g_queue_get_length(handlers);
	for (i=0;i<num_handlers;i++) {
		h = g_queue_peek_nth(handlers, i);
		g_print("handlers: init %s\n",h->name);

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

	g_free(h->name);
	g_free(h);

	return;
}

void handlers_finalize(GQueue *handlers) {
	g_print("handlers: finalizing handlers.\n");
	g_queue_foreach(handlers, handlers_finalize_single, NULL);
	return;
}

struct handler *handler_new(gchar *name) {
	struct handler *h;

	h = NULL;

	if (!name) {
		g_print("An handler can not have a NULL name.\n");
		return h;
	}

	h = g_malloc0(sizeof(struct handler));

	h->name = g_strdup(name);
	return h;
}

void handler_enable(struct handler *h, gboolean enabled) {
	h->enabled = enabled;
	return;
}

void handler_set_initialize(struct handler *h, void (*handler_initialize_cb)(gpointer data)) {
	h->handler_initialize = handler_initialize_cb;
	return;
}

void handler_set_handle(struct handler *h, gpointer (*handler_handle_cb)(gpointer data, gpointer hmessage)) {
	if (!handler_handle_cb) {
		g_print("An handler will a NULL cb ? Why?\n");
		return;
	}

	h->handle = handler_handle_cb;
	return;
}

void handler_set_finalize(struct handler *h, void (*handler_finalize_cb)(gpointer data)) {
	h->handler_finalize = handler_finalize_cb;
	return;
}

struct handler *handler_find_by_name(GQueue *handlers, gchar *n) {
	struct handler *h;
	guint i;
	guint hl;

	h = NULL;
	hl = 0;

	if ((!n) || (!handlers))
		return h;

	hl = g_queue_get_length(handlers);
	if (!hl)
		return h;

	for (i=0;i<hl;i++) {
		h = g_queue_peek_nth(handlers, i);

		if (!g_strcmp0(h->name, n))
			break;

	}

	if (i == hl)
		h = NULL;

	return h;
}
