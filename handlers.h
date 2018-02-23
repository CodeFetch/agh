#ifndef __callbacks_h__
#define __callbacks_h__

#include <glib.h>

struct handler {
	gboolean enabled;
	gboolean on_stack;

	void (*handler_initialize)(gpointer data);
	gpointer (*handle)(gpointer data, gpointer hmessage);
	void (*handler_finalize)(gpointer data);

	/* A pointer to the hanler queue. Handlers should avoid accessing their queue still if at all possible. */
	GQueue *handlers_queue;

	/* private handler data */
	gpointer handler_data;
};

GQueue *handlers_setup(void);
void handler_register(GQueue *handlers, struct handler *h);
void handlers_init(GQueue *handlers);
void handlers_finalize(GQueue *handlers);
void handlers_finalize_single(gpointer data, gpointer user_data);
void handlers_teardown(GQueue *handlers);

#endif
