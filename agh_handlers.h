#ifndef __agh_handlers_h__
#define __agh_handlers_h__

#include <glib.h>

struct handler {
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
GQueue *handlers_setup(void);
void handler_register(GQueue *handlers, struct handler *h);
void handlers_init(GQueue *handlers, gpointer data);
void handlers_finalize(GQueue *handlers);
void handlers_finalize_single(gpointer data, gpointer user_data);
void handlers_teardown(GQueue *handlers);

/* handlers structures helpers */
struct handler *handler_new(gchar *name);
void handler_enable(struct handler *h, gboolean enabled);
void handler_set_initialize(struct handler *h, void (*handler_initialize_cb)(gpointer data));
void handler_set_handle(struct handler *h, gpointer (*handler_handle_cb)(gpointer data, gpointer hmessage));
void handler_set_finalize(struct handler *h, void (*handler_finalize_cb)(gpointer data));
struct handler *handler_find_by_name(GQueue *handlers, gchar *n);

#endif