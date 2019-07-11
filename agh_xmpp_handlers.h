/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __agh_xmpp_handlers_h__
#define __agh_xmpp_handlers_h__

struct agh_message *xmpp_sendmsg_handle(struct agh_handler *h, struct agh_message *m);
struct agh_message *xmpp_cmd_handle(struct agh_handler *h, struct agh_message *m);

#endif
