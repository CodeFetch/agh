#ifndef __agh_handlers_h__
#define __agh_handlers_h__

#include <glib.h>

struct agh_handler {
	gboolean enabled;
	gchar *name;

	void (*handler_initialize)(gpointer data);
	gpointer (*handle)(gpointer data, gpointer hmessage);
	void (*handler_finalize)(gpointer data);

	/* A pointer to the hanler queue. Handlers should avoid accessing their queue still if at all possible. */
	GQueue *handlers_queue;

	/* private handler data */
	gpointer handler_data;
};

/* Those functions are declared in the order they need to be used. */
GQueue *agh_handlers_setup(void);
gint agh_handler_register(GQueue *handlers, struct agh_handler *h);
gint agh_handlers_init(GQueue *handlers, gpointer data);
void agh_handlers_finalize(GQueue *handlers);
gint agh_handlers_teardown(GQueue *handlers);

/* handlers structures helpers */
struct agh_handler *agh_new_handler(gchar *name);
gint agh_handler_enable(struct agh_handler *h, gboolean enabled);
gint agh_handler_set_initialize(struct agh_handler *h, void (*handler_initialize_cb)(gpointer data));
gint agh_handler_set_handle(struct agh_handler *h, gpointer (*handler_handle_cb)(gpointer data, gpointer hmessage));
gint agh_handler_set_finalize(struct agh_handler *h, void (*handler_finalize_cb)(gpointer data));

#endif
