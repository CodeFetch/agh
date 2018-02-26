#ifndef __messages_h__
#define __messages_h__
#include <glib.h>
#include "agh.h"

/* Messages opcodes */
#define CORE_SHUTDOWN						0
#define XMPP_SEND_MSG 					1
#define XMPP_TRANSMIT_DATA 			2
#define CORE_TEXTCOMMAND				3

struct agh_message {
	gsize csp_len;
	guint opcode;
	GAsyncQueue *src_comm;
	GAsyncQueue *dest_comm;
	
	gpointer csp;
};

struct agh_message *msg_alloc(gsize len);
void msg_dealloc(struct agh_message *m);
void msg_prepare(struct agh_message *m, GAsyncQueue *src_comm, GAsyncQueue *dest_comm);
void msg_send(struct agh_message *m);
#endif
