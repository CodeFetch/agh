#ifndef __aghservices_h__
#define __aghservices_h__

void aghservices_messaging_setup(struct agh_thread *ct);

gboolean aghservices_get_message(gpointer data);
#endif