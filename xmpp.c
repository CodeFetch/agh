#include <glib.h>

gpointer xmpp_start(gpointer data) {
	g_print("XMPP setup here.\n");
	return data;
}
