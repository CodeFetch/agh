#ifndef __aghservices_h__
#define __aghservices_h__
#include "messages.h"

void aghservices_messaging_setup(struct agh_thread *ct, GQueue *handlers);
gboolean aghservices_receive_messages(gpointer data);
void aghservices_handle_message(GQueue *handlers, struct agh_message *m);
#endif