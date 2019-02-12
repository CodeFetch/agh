/* handlers */
#include <glib.h>
#include "agh_messages.h"
#include "agh_handlers.h"
#include "agh_logging.h"

/* Log messages from core domain. */
#define AGH_LOG_DOMAIN_HANDLERS	"HANDLERS"
#define agh_log_handlers_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_HANDLERS, message, ##__VA_ARGS__)
#define agh_log_handlers_info(message, ...) agh_log_info(AGH_LOG_DOMAIN_HANDLERS, message, ##__VA_ARGS__)
#define agh_log_handlers_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_HANDLERS, message, ##__VA_ARGS__)

/* Function prototypes. */
static void agh_handlers_finalize_single(gpointer data, gpointer user_data);

/*
 * Allocates handlers queue with g_queue_new.
 * At the time of this writing, g_queue_new uses g_slice_alloc, which in turn calls g_malloc internally, hence a failure to allocate memory may lead to program termination.
*/
GQueue *agh_handlers_setup(void) {
	GQueue *hq;

	hq = g_queue_new();

	return hq;
}

/*
 * agh_handlers_teardown: deallocates an AGH handlers queue.
 *
 * We did not foresee a real "failure case" here. The function tries to alert you about the fact that some handlers where
 * still registered when you called it.
 *
 * Returns: an integer with value 0 on success, -1 if passed in GQueue was NULL, -1 if some handlers where still registered
*/
gint agh_handlers_teardown(GQueue *handlers) {
	guint num_handlers;
	gint retval;

	retval = 0;

	if (!handlers) {
		agh_log_handlers_crit("the passed in AGH handlers queue was NULL");
		retval--;
		return retval;
	}

	num_handlers = g_queue_get_length(handlers);

	if (num_handlers) {
		agh_log_handlers_dbg("%" G_GUINT16_FORMAT" handlers where still registed! Trying to continue.",num_handlers);
		g_queue_foreach(handlers, agh_handlers_finalize_single, NULL);
		retval = -2;
	}

	g_queue_free(handlers);

	return retval;
}

/*
 * Adds ("registers") an AGH handler to a handlers queue, so it can be initialized (if needed), and invoked later.
 *
 * Returns: an integer with value -1 on failure, 0 otherwise.
*/
gint agh_handler_register(GQueue *handlers, struct agh_handler *h) {
	gint ret;

	ret = 0;

	if ((!h) || (!handlers)) {
		agh_log_handlers_crit("can not register a NULL AGH handler, or add an AGH handler to a NULL handlers queue");
		ret = -1;
	}
	else {
		g_queue_push_tail(handlers, h);
	}

	return ret;
}

/* Initialize AGH handlers in this queue.
 * Each handler's init function is invoked, if the handler is actually enabled and the callback is not NULL.
 * Some handler's structure data fields are initialized in any case.
 *
 * Returns: an integer<0 on failure, integer>=0 otherwise.
*/
gint agh_handlers_init(GQueue *handlers, gpointer data) {
	guint i;
	guint num_handlers;
	struct agh_handler *h;

	num_handlers = -1;

	if (!handlers) {
		agh_log_handlers_crit("passed in a NULL AGH handlers queue");
		return num_handlers;
	}

	num_handlers = g_queue_get_length(handlers);
	for (i=0;i<num_handlers;i++) {
		h = g_queue_peek_nth(handlers, i);

		h->handlers_queue = handlers;

		if (data)
			h->handler_data = data;

		if (h->enabled && h->handler_initialize) {
			agh_log_handlers_dbg("AGH handler init cb for %s being invoked",h->name);
			h->handler_initialize(h);
		}
	}

	return num_handlers;
}

/*
 * Invoke the handler_finalize callback for an AGH handler.
 *
 * Returns: nothing; it's invoked via the g_queue_free_foreach GLib function.
*/
static void agh_handlers_finalize_single(gpointer data, gpointer user_data) {
	struct agh_handler *h = data;

	if (h->enabled && h->handler_finalize) {
		agh_log_handlers_dbg("AGH handler finalize cb for %s being invoked",h->name);
		h->handler_finalize(h);
	}

	g_queue_remove(h->handlers_queue, h);
	h->handlers_queue = NULL;

	h->handler_data = NULL;

	agh_log_handlers_dbg("AGH handler %s being deallocated",h->name);
	g_free(h->name);
	g_free(h);

	return;
}

/*
 * Given an AGH handlers queue, finalize all of the linked handlers.
*/
void agh_handlers_finalize(GQueue *handlers) {
	agh_log_handlers_dbg("finalizing handlers");
	g_queue_foreach(handlers, agh_handlers_finalize_single, NULL);
	return;
}

/*
 * Allocates a new AGH handler with the given name.
 *
 * Returns: a new handler data structure on success, NULL on failure.
 *
 * Note: this function may terminate AGH should an allocation failure occur in g_strdup.
*/
struct agh_handler *agh_new_handler(gchar *name) {
	struct agh_handler *h;

	h = NULL;

	if (!name) {
		agh_log_handlers_crit("an AGH handler can not have a NULL name");
		return h;
	}

	h = g_try_malloc0(sizeof(*h));
	if (!h) {
		agh_log_handlers_crit("memory allocation failure while allocating an AGH handler");
		return h;
	}

	h->name = g_strdup(name);
	return h;
}

/*
 * Changes an AGH handler "enabled" state.
*/
gint agh_handler_enable(struct agh_handler *h, gboolean enabled) {
	gint retval;

	retval = 0;

	if (!h) {
		agh_log_handlers_crit("can not enable or disable a NULL AGH handler");
		retval--;
	}
	else
		h->enabled = enabled;

	return retval;
}

/*
 * Assigns to an AGH handler an init callback.
 *
 * Returns: an integer with value -1 if the passed in handler (or callback) are NULL, 0 otherwise.
*/
gint agh_handler_set_initialize(struct agh_handler *h, void (*handler_initialize_cb)(gpointer data)) {
	gint retval;

	retval = 0;

	if (!h || !handler_initialize_cb) {
		agh_log_handlers_crit("can not assign an init callback to a NULL AGH handler, or a NULL callback to a valid one");
		retval--;
	}
	else
		h->handler_initialize = handler_initialize_cb;

	return retval;
}

/*
 * Sets the callback for an AGH handler.
 *
 * Returns: an integer value of 0 on success, -1 on failure (e.g.: handler or callback where NULL).
*/
gint agh_handler_set_handle(struct agh_handler *h, gpointer (*handler_handle_cb)(gpointer data, gpointer hmessage)) {
	gint retval;

	retval = 0;

	if (!h || !handler_handle_cb) {
		agh_log_handlers_crit("a NULL AGH handler, or a NULL callback, are not considered legal");
		retval--;
	}
	else
		h->handle = handler_handle_cb;

	return retval;
}

/*
 * Assigns a finalize callback to an AGH handler.
 *
 * Returns: an integer with value 0 on success, -1 otherwise (e.g.: NULL handler or callback).
*/
gint agh_handler_set_finalize(struct agh_handler *h, void (*handler_finalize_cb)(gpointer data)) {
	gint retval;

	retval = 0;

	if (!h || !handler_finalize_cb) {
		agh_log_handlers_crit("a NULL AGH handler, or a NULL callback, are not considered legal");
		retval--;
	}
	else
		h->handler_finalize = handler_finalize_cb;

	return retval;
}
