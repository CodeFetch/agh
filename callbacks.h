#ifndef __callbacks_h__
#define __callbacks_h__

#include <glib.h>
#include "agh.h"

struct test_csp {
	guint num;
	char payload[700];
};

struct handler {
	void (*handler_initialize)(void);
	void (*handle)(void);
	void (*handler_finalize)(void);
};

gboolean agh_unix_signals_cb_dispatch(gpointer data);
gboolean agh_timeout_cb_dispatch(gpointer data);
void agh_threads_test_sendmsg(gpointer data, gpointer user_data);
#endif
