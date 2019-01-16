#include <string.h>
#include <glib.h>
#include <nettle/sha1.h>
#include "agh_xmpp_caps.h"
#include "agh_xmpp.h"
#include "agh.h"

static void agh_xmpp_caps_base_entities_free(gpointer data);
static gchar *agh_xmpp_caps_sha1(gchar *text);
static gchar *agh_xmpp_caps_build_string(struct agh_xmpp_caps_entity *e);
static void agh_xmpp_caps_base_entity_to_string(gpointer data, gpointer user_data);
static gint agh_xmpp_caps_gcmp0_wrapper(gconstpointer a, gconstpointer b, gpointer user_data);

/* I am not convinced about the correctness of this function. */
static gchar *agh_xmpp_caps_sha1(gchar *text) {
	struct sha1_ctx *ctx;
	uint8_t *input;
	uint8_t *digest;
	gchar *output;
	gint text_len;

	ctx = NULL;
	output = NULL;
	text_len = 0;

	if (!text)
		return NULL;

	text_len = strlen(text);
	ctx = g_malloc0(sizeof(struct sha1_ctx));
	input = g_malloc0(text_len+1);
	digest = g_malloc0(SHA1_DIGEST_SIZE);
	memcpy(input, text, text_len+1);

	sha1_init(ctx);
	sha1_update(ctx, text_len, input);
	sha1_digest(ctx, SHA1_DIGEST_SIZE, digest);

	output = g_base64_encode(digest, SHA1_DIGEST_SIZE);

	g_free(digest);
	g_free(input);
	g_free(ctx);

	return output;
}

struct agh_xmpp_caps_entity *agh_xmpp_caps_entity_alloc(void) {
	struct agh_xmpp_caps_entity *e;

	e = NULL;

	e = g_malloc0(sizeof(struct agh_xmpp_caps_entity));

	e->features = g_queue_new();
	e->base_entities = g_queue_new();

	return e;
}

void agh_xmpp_caps_entity_dealloc(struct agh_xmpp_caps_entity *e) {

	if (!e)
		return;

	if (e->base_entities)
		g_queue_free_full(e->base_entities, agh_xmpp_caps_base_entities_free);
	g_free(e->softname);
	g_free(e->softversion);
	g_free(e->osname);
	g_free(e->osversion);
	if (e->features)
		g_queue_free_full(e->features, g_free);
	g_free(e);

	return;
}

static void agh_xmpp_caps_base_entities_free(gpointer data) {
	struct agh_xmpp_caps_base_entity *b = data;

	g_free(b->name);
	g_free(b->cat);
	g_free(b->type);
	g_free(b->lang);
	g_free(b);
}

gint agh_xmpp_caps_add_entity(struct agh_xmpp_caps_entity *e) {
	struct agh_xmpp_caps_base_entity *b;

	b = NULL;

	if (!e)
		return -1;

	b = g_malloc0(sizeof(struct agh_xmpp_caps_base_entity));
	g_queue_push_tail(e->base_entities, b);

	return (g_queue_get_length(e->base_entities)-1);
}

void agh_xmpp_caps_set_entity_data(struct agh_xmpp_caps_entity *e, gint id, gchar *name, gchar *type, gchar *cat, gchar *lang) {
	struct agh_xmpp_caps_base_entity *b;

	b = NULL;

	if (!name || !type)
		return;

	if (!cat)
		return;

	if ((!e) || (!e->base_entities))
		return;

	if (id < 0)
		return;

	b = g_queue_peek_nth(e->base_entities, id);
	if (!b)
		return;

	b->name = g_strdup(name);

	b->type = g_strdup(type);

	b->cat = g_strdup(cat);

	if (lang)
		b->lang = g_strdup(lang);

	return;
}

static gchar *agh_xmpp_caps_build_string(struct agh_xmpp_caps_entity *e) {
	gchar *tmp_str;
	GString *o;
	GQueue *entities_str;
	GQueue *features_str;
	guint i;
	guint num_elems;

	tmp_str = NULL;
	o = NULL;
	entities_str = NULL;
	features_str = NULL;
	i = 0;
	num_elems = 0;

	if ((!e) || (!e->base_entities))
		return NULL;

	features_str = g_queue_new();
	entities_str = g_queue_new();
	o = g_string_new(NULL);

	g_queue_foreach(e->base_entities, agh_xmpp_caps_base_entity_to_string, entities_str);

	if (e->features)
		g_queue_foreach(e->features, agh_copy_textparts, features_str);

	g_queue_sort(entities_str, agh_xmpp_caps_gcmp0_wrapper, NULL);
	g_queue_sort(features_str, agh_xmpp_caps_gcmp0_wrapper, NULL);

	num_elems = g_queue_get_length(entities_str);

	for (i=0;i<num_elems;i++) {
		tmp_str = g_queue_peek_nth(entities_str, i);
		g_string_append(o, tmp_str);
	}

	num_elems = g_queue_get_length(features_str);

	for (i=0;i<num_elems;i++) {
		tmp_str = g_queue_peek_nth(features_str, i);
		g_string_append_printf(o, "%s<", tmp_str);
	}

	g_queue_free_full(entities_str, g_free);
	g_queue_free_full(features_str, g_free);

	entities_str = NULL;
	features_str = NULL;

	return g_string_free(o, FALSE);
}

gint agh_xmpp_caps_add_feature(struct agh_xmpp_caps_entity *e, gchar *ftext) {
	if ((!e) || (!ftext))
		return -1;

	g_queue_push_tail(e->features, g_strdup(ftext));

	return (g_queue_get_length(e->features)-1);
}

static void agh_xmpp_caps_base_entity_to_string(gpointer data, gpointer user_data) {
	struct agh_xmpp_caps_base_entity *b = data;
	GQueue *dest = user_data;
	GString *tmp;

	tmp = g_string_new(NULL);

	g_string_append_printf(tmp, "%s/%s/%s/%s<",b->cat, b->type, b->lang ? b->lang : "", b->name);

	g_queue_push_tail(dest, g_string_free(tmp, FALSE));

	return;
}

void agh_xmpp_caps_add_hash(xmpp_ctx_t *ctx, struct agh_xmpp_caps_entity *e, xmpp_stanza_t *pres) {
	xmpp_stanza_t *caps = NULL;
	gchar *hash;
	gchar *str;

	str = NULL;
	hash = NULL;

	if (!e)
		return;

	str = agh_xmpp_caps_build_string(e);

	if (!str)
		return;

	hash = agh_xmpp_caps_sha1(str);

	if (!hash) {

		if (str) {
			g_free(str);
			str = NULL;
		}

		g_print("%s: NULL hash\n",__FUNCTION__);
		return;
	}

	caps = xmpp_stanza_new(ctx);
	xmpp_stanza_set_name(caps, AGH_XMPP_STANZA_NAME_C);
	xmpp_stanza_set_ns(caps, AGH_XMPP_STANZA_NS_CAPS);
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_HASH, "sha-1");
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_NODE, "http://meizo.net");
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_VER, hash);
	xmpp_stanza_add_child(pres, caps);
	xmpp_stanza_release(caps);
	g_free(hash);
	hash = NULL;
	g_free(str);
	str = NULL;
	g_print("%s: exiting\n",__FUNCTION__);
	return;
}

xmpp_stanza_t *agh_xmpp_caps_get_capsdata(struct xmpp_state *xstate) {
	xmpp_stanza_t *capsdata;
	xmpp_stanza_t *identity;
	xmpp_stanza_t *feature;
	guint i;
	guint num_elems;
	struct agh_xmpp_caps_base_entity *b;
	gchar *ftext;

	capsdata = NULL;
	identity = NULL;
	i = 0;
	num_elems = 0;
	b = NULL;
	feature = NULL;
	ftext = NULL;

	if ((!xstate->e) || (!xstate->e->base_entities))
		return NULL;

	capsdata = xmpp_stanza_new(xstate->xmpp_ctx);

	xmpp_stanza_set_name(capsdata, AGH_XMPP_STANZA_NAME_QUERY);

	xmpp_stanza_set_ns(capsdata, XMPP_NS_DISCO_INFO);

	num_elems = g_queue_get_length(xstate->e->base_entities);

	for (i=0;i<num_elems;i++) {
		identity = xmpp_stanza_new(xstate->xmpp_ctx);
		xmpp_stanza_set_name(identity, "identity");

		b = g_queue_peek_nth(xstate->e->base_entities, i);

		xmpp_stanza_set_attribute(identity, "category", b->cat);
		xmpp_stanza_set_type(identity, b->type);
		xmpp_stanza_set_attribute(identity, "name", b->name);

		if (b->lang)
			xmpp_stanza_set_attribute(identity, "lang", b->lang);

		xmpp_stanza_add_child(capsdata, identity);
		xmpp_stanza_release(identity);
	}

	num_elems = g_queue_get_length(xstate->e->features);

	for (i=0;i<num_elems;i++) {
		feature = xmpp_stanza_new(xstate->xmpp_ctx);
		xmpp_stanza_set_name(feature, AGH_XMPP_STANZA_NAME_FEATURE);
		ftext = g_queue_peek_nth(xstate->e->features, i);
		xmpp_stanza_set_attribute(feature, AGH_XMPP_STANZA_ATTR_VAR, ftext);
		xmpp_stanza_add_child(capsdata, feature);
		xmpp_stanza_release(feature);
	}

	return capsdata;
}

/*
 * Only to make GCC 8+ happy. And in any case, stay on the safe side for now, avoiding to play with function casts.
*/
static gint agh_xmpp_caps_gcmp0_wrapper(gconstpointer a, gconstpointer b, gpointer user_data) {
	const gchar *f = a;
	const gchar *g = b;

	return g_strcmp0(f, g);
}
