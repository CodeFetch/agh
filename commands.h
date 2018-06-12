#ifndef __commands_h__
#include <glib.h>
#define __commands_h__
#include "messages.h"

struct command {
	GAsyncQueue *resdest;
	gulong cmd_id;
	gchar *operation;
	GQueue *argq;
};

gpointer cmd_process_msgtext(struct agh_message *m);

#endif
