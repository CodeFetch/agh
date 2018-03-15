#ifndef __commands_h__
#include <glib.h>
#define __commands_h__

struct command_csp {
	GAsyncQueue *dest_text;
	gulong cmd_id;
	gchar *operation;
	GQueue *argq;
};

struct command_handler {
	gchar *operation;
	gpointer function;
};

#endif
