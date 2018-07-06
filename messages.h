#ifndef __messages_h__
#define __messages_h__
#include <glib.h>
#include "agh.h"

/* Message types */
#define MSG_RECVTEXT 						0
#define MSG_SENDTEXT						1
#define MSG_SENDCMD							3
#define MSG_EVENT							4


struct agh_message {
	guint msg_type;
	GAsyncQueue *src_comm;
	GAsyncQueue *dest_comm;
	gpointer csp;
};

struct agh_message *msg_alloc(void);
void msg_dealloc(struct agh_message *m);
guint msg_prepare(struct agh_message *m, GAsyncQueue *src_comm, GAsyncQueue *dest_comm);
void msg_send(struct agh_message *m);
void msg_dealloc_from_queue(gpointer data);
#endif
