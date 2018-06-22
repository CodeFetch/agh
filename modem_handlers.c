#include "handlers.h"
#include "messages.h"
#include "commands.h"
#include "modem.h"
#include "modem_handlers.h"

gpointer modem_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_thread *ct = h->handler_data;
	struct modem_state *mmstate = ct->thread_data;
	struct command *cmd;
	const gchar *string_arg;
	gint current_modem;
	config_setting_t *arg;

	cmd = NULL;
	string_arg = NULL;
	current_modem = 0;
	arg = NULL;

	if (m->msg_type != MSG_SENDCMD)
		return cmd;

	/* If can not act on commands if talking with ModemManager is not possible. */
	if (!mmstate->manager)
		return cmd;

	cmd = m->csp;

	if (g_strcmp0(cmd_get_operation(cmd), CMD_MODEM_OP))
		return NULL;

	/* If an integer was specified, then this is the modem on which we're supposed to operate. Otherwise it's a subcommand. */
	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_INT);
	if (arg) {
		/* A modem was specified. */
		current_modem = config_setting_get_int(arg);
		g_print("Modem %" G_GINT16_FORMAT" has been specified.\n",current_modem);
		/* subcommands requiring a modem argument should be called from here */
		return NULL;
	}

	arg = cmd_get_arg(cmd, 1, CONFIG_TYPE_STRING);

	if (arg) {
		string_arg = config_setting_get_string(arg);
		g_print("General subcommand was specified: %s\n",string_arg);
		return NULL;
	}

	g_print("Listing modems here.\n");

	return NULL;
}
