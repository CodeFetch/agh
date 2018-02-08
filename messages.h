#ifndef __messages_h__
#define __messages_h__
#include <glib.h>
#include "agh.h"

struct agh_message {
	gsize len;
	struct agh_thread *src_ct;
	gpointer csp;
};

struct agh_message *msg_alloc(gsize len);
void msg_dealloc(struct agh_message *m);

#endif
