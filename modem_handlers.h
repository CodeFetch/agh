#ifndef __modem_handlers_h__
#define __modem_handlers_h__

/* String error responses. */
#define AGH_MM_MSG_DATA_NOT_AVAILABLE "NOT_AVAILABLE"
#define AGH_MM_MSG_IMEI_NOT_AVAILABLE "IMEI_NOT_AVAILABLE"
#define NO_MMModem3gpp_OBJECT "NO_MMModem3gpp_OBJECT"
#define NO_MMModem_OBJECT "NO_MMModem_OBJECT"

/* Operations */
#define AGH_CMD_MODEM	"modem"

/* subcommands */
#define AGH_MM_GET_IMEI "get_imei"

gpointer modem_cmd_handle(gpointer data, gpointer hmessage);

void agh_mm_list_modems(struct modem_state *mmstate, struct command *cmd);
void agh_mm_list_modem_single(gpointer data, gpointer user_data);
gchar *agh_mm_modem_to_index(const gchar *modem_path);
MMObject *agh_mm_index_to_modem(struct modem_state *mmstate, gint modem_index);
void agh_modem_do(MMObject *modem, struct command *cmd);

/* MMModem3gpp */
MMModem3gpp *agh_get_MMModem3gpp_object(MMObject *modem, struct command *cmd);
void agh_modem_get_imei(MMObject *modem, struct command *cmd);

/* MMModem */
MMModem *agh_get_MMModem_object(MMObject *modem, struct command *cmd);

#endif
