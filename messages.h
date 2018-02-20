#ifndef __messages_h__
#define __messages_h__
#include <glib.h>
#include "agh.h"

struct agh_message {
	gsize csp_len;
	GAsyncQueue *src_comm;
	GAsyncQueue *dest_comm;
	
	gpointer csp;
};

struct agh_message *msg_alloc(gsize len);
void msg_dealloc(struct agh_message *m);
void msg_prepare(struct agh_message *m, GAsyncQueue *src_comm, GAsyncQueue *dest_comm);
void msg_send(struct agh_message *m);
#endif
