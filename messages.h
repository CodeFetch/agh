#ifndef __messages_h__
#define __messages_h__
#include <glib.h>
#include "agh.h"

/* Message types. */
#define MSG_INVALID							0
#define MSG_RECVTEXT 						1
#define MSG_SENDTEXT						2
#define MSG_SENDCMD							3
#define MSG_EVENT								4
#define MSG_EXIT								5
/* End of message types. */

/* GMainContext iteration on each thread before concluding no one will send messages anymore. */
#define AGH_MAX_MESSAGEWAIT_ITERATIONS 380

/*
 * Why the GMainContext *src_ctx struct member?
 * To allow handlers to answer a message with another, simply returning it.
 * Suggestions on ways to to this betterare welcome.
*/
struct agh_message {
	guint msg_type;
	struct agh_comm *src;
	struct agh_comm *dest;
	gpointer csp;
};

struct agh_comm {
	GQueue *handlers;
	GMainContext *ctx;
	gchar *name;
	gboolean teardown_in_progress;
};

struct agh_message *msg_alloc(void);
void msg_dealloc(struct agh_message *m);
gint msg_send(struct agh_message *m, struct agh_comm *src_comm, struct agh_comm *dest_comm);
gboolean agh_handle_message_inside_dest_thread(gpointer data);

/* comm */
struct agh_comm *agh_comm_setup(GQueue *handlers, GMainContext *ctx, gchar *name);
void agh_comm_teardown(struct agh_comm *comm);
void agh_comm_disable(struct agh_comm *comm, gboolean enabled);

#endif
