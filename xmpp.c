#include <glib.h>

void xmpp_thread_init(gpointer data) {
	g_print("XMPP thread init function has been invoked.\n");
	return;
}

gpointer xmpp_thread_start(gpointer data) {
	g_print("XMPP started.\n");
	return data;
}

void xmpp_thread_deinit(gpointer data) {
	g_print("XMPP deinit.\n");
	return;
}
