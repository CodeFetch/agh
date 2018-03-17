#include <string.h>
#include "commands.h"
#include "messages.h"

gpointer cmd_process_msgtext(struct agh_message *m) {
	struct text_csp *mycsp = m->csp;

	g_print("Command received: %" G_GSIZE_FORMAT ", %s.\n",strlen(mycsp->text), mycsp->text);

	g_free(mycsp->text);
	return NULL;
}
