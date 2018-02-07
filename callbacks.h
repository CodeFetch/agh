#ifndef __callbacks_h__
#define __callbacks_h__

#include <glib.h>
#include "agh.h"

struct test_csp {
	guint num;
	char payload[700];
};

struct handler {
	gboolean enabled;

	void (*handler_initialize)(gpointer data);
	void (*handle)(void);
	void (*handler_finalize)(gpointer data);
};

gboolean agh_unix_signals_cb_dispatch(gpointer data);
gboolean agh_timeout_cb_dispatch(gpointer data);
void agh_threads_test_sendmsg(gpointer data, gpointer user_data);

GQueue *handlers_setup(void);
void handler_register(GQueue *handlers, struct handler *h);
void handlers_init(GQueue *handlers, gpointer data);
void handlers_init_single(gpointer data, gpointer user_data);
void handler_unregister(GQueue *handlers, struct handler *h, gpointer data);
void handlers_finalize(GQueue *handlers, gpointer data);
void handlers_finalize_single(gpointer data, gpointer user_data);
void handlers_teardown(GQueue *handlers);
#endif
