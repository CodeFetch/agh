#ifndef __aghservices_h__
#define __aghservices_h__
#include "messages.h"

void aghservices_messaging_setup(struct agh_thread *ct);
gboolean aghservices_receive_messages(gpointer data);
void aghservices_handle_message(GQueue *handlers, struct agh_message *m, GAsyncQueue *src_comm);
void aghservices_core_messaging_setup(struct agh_state *mstate);
gboolean aghservices_core_receive_messages(gpointer data);
void aghservices_common_messaging_setup(GSource *evsrc, GSourceFunc evsrc_callback, gpointer data, guint *tag, GMainContext *ctx);
void aghservices_common_receive_messages(GAsyncQueue *comm, GQueue *handlers);
#endif