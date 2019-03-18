#ifndef __agh_ubus_handler_H__
#define __agh_ubus_handler_H__
#include "libubus.h"

/* Operations. */
#define AGH_CMD_UBUS "ubus"
/* End of operations. */

/* AGH_CMD_UBUS subcommands. */
#define AGH_CMD_UBUS_LIST "list"
#define AGH_CMD_UBUS_CALL "call"
#define AGH_CMD_UBUS_LISTEN "events"
#define AGH_CMD_UBUS_LOGSTREAM "logstream"
/* End of AGH_CMD_UBUS subcommands. */

/* AGH_CMD_UBUS_LISTEN subcommands */
#define AGH_CMD_UBUS_LISTEN_ADD "add"
#define AGH_CMD_UBUS_LISTEN_STOP "reset"
/* end of AGH_CMD_UBUS_LISTEN subcommands */

/* AGH_CMD_UBUS_LOGSTREAM subcommands. */
#define AGH_CMD_UBUS_LOGSTREAM_ACTIVATE "+"
#define AGH_CMD_UBUS_LOGSTREAM_DEACTIVATE "-"
/* End of AGH_CMD_UBUS_LOGSTREAM subcommands. */

/* ubus events event name */
#define AGH_UBUS_HANDLER_UBUS_EVENTs_NAME "UBUS_EVENT"

struct agh_message *agh_core_ubus_cmd_handle(struct agh_handler *h, struct agh_message *m);

#endif
