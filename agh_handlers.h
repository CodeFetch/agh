/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __agh_handlers_h__
#define __agh_handlers_h__

#include <glib.h>

struct agh_handler;

typedef void *(agh_handler_init_cb)(struct agh_handler *myself);
typedef struct agh_message *(agh_handler_handle_cb)(struct agh_handler *myself, struct agh_message *m);
typedef void *(agh_handler_finalize_cb)(struct agh_handler *myself);

struct agh_handler {
	gboolean enabled;
	gchar *name;

	//void (*handler_initialize)(struct agh_handler *h);
	agh_handler_init_cb *handler_initialize;
	//struct agh_message (*handle)(struct agh_handler *h, struct agh_message *hmessage);
	agh_handler_handle_cb *handle;
	//void (*handler_finalize)(struct agh_handler *h);
	agh_handler_finalize_cb *handler_finalize;

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
gint agh_handler_set_initialize(struct agh_handler *h, agh_handler_init_cb *handler_initialize_cb);
gint agh_handler_set_handle(struct agh_handler *h, agh_handler_handle_cb *handler_handle_cb);
gint agh_handler_set_finalize(struct agh_handler *h, agh_handler_finalize_cb *handler_finalize_cb);
gint agh_handler_dealloc(struct agh_handler *h);

#endif
