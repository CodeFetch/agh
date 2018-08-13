#ifndef __agh_xmpp_caps_h__
#define __agh_xmpp_caps_h__
#include <strophe.h>
#include "agh_xmpp.h"

struct agh_xmpp_caps_base_entity {
	gchar *name;
	gchar *lang;
	gchar *type;
	gchar *cat;
};

struct agh_xmpp_caps_entity {
	GQueue *base_entities;
	gchar *softname;
	gchar *softversion;
	gchar *osname;
	gchar *osversion;
	GQueue *features;
};

struct agh_xmpp_caps_entity *agh_xmpp_caps_entity_alloc(void);
void agh_xmpp_caps_entity_dealloc(struct agh_xmpp_caps_entity *e);
void agh_xmpp_caps_base_entities_free(gpointer data);

gint agh_xmpp_caps_add_entity(struct agh_xmpp_caps_entity *e);
void agh_xmpp_caps_set_entity_data(struct agh_xmpp_caps_entity *e, gint id, gchar *name, gchar *type, gchar *cat, gchar *lang);

gint agh_xmpp_caps_add_feature(struct agh_xmpp_caps_entity *e, gchar *ftext);

gchar *agh_xmpp_caps_sha1(gchar *text);
gchar *agh_xmpp_caps_build_string(struct agh_xmpp_caps_entity *e);
void agh_xmpp_caps_base_entity_to_string(gpointer data, gpointer user_data);
void agh_xmpp_caps_add_hash(xmpp_ctx_t *ctx, struct agh_xmpp_caps_entity *e, xmpp_stanza_t *pres);
xmpp_stanza_t *agh_xmpp_caps_get_capsdata(struct xmpp_state *xstate);

#endif
