#ifndef __messages_h__
#define __messages_h__
#include <glib.h>

struct agh_message {
	gsize len;
	void *csp;
};

struct agh_message *msg_alloc(gsize len);
void msg_dealloc(struct agh_message *m);

#endif
