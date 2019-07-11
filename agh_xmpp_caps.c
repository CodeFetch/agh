// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Here is some code to support XMPP Capabilities.
*/

#include <string.h>
#include <glib.h>
#include <nettle/sha1.h>
#include "agh_xmpp_caps.h"
#include "agh_xmpp.h"
#include "agh.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_XMPP domain. */
#define AGH_LOG_DOMAIN_XMPPCAPS	"XMPPCAPS"

/* Logging macros. */
#define agh_log_xmppcaps_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_XMPPCAPS, message, ##__VA_ARGS__)
#define agh_log_xmppcaps_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_XMPPCAPS, message, ##__VA_ARGS__)

/*
 * Uses libnettle to calculate SHA1 within the process of "publishing" XMPP Capabilities (I call them "caps").
 *
 * Returns: the SHA1 on success, NULL when an allocation failure occurs and is handled.
 * Infact, this function may still lead to an unclean program termination.
 *
 * Note: I am not convinced about the correctness of this function.
*/
static gchar *agh_xmpp_caps_sha1(gchar *text) {
	struct sha1_ctx *ctx;
	uint8_t *input;
	uint8_t *digest;
	gchar *output;
	gint text_len;

	output = NULL;

	if (!text) {
		agh_log_xmppcaps_crit("NULL text");
		return output;
	}

	text_len = strlen(text);
	if (!text_len) {
		agh_log_xmppcaps_crit("text of 0 length");
		return output;
	}

	ctx = g_try_malloc0(sizeof(*ctx));
	if (!ctx) {
		agh_log_xmppcaps_crit("failure while allocating sha1_ctx");
		return output;
	}

	input = g_try_malloc0(text_len+1);
	if (!input) {
		agh_log_xmppcaps_crit("failure while allocating input");
		g_free(ctx);
		return output;
	}

	digest = g_try_malloc0(SHA1_DIGEST_SIZE);
	if (!digest) {
		agh_log_xmppcaps_crit("failure while allocating digest");
		g_free(ctx);
		g_free(input);
		return output;
	}

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

	e = g_try_malloc0(sizeof(*e));
	if (!e) {
		agh_log_xmppcaps_crit("failure while allocating struct agh_xmpp_caps_entity");
		return e;
	}

	e->features = g_queue_new();
	e->base_entities = g_queue_new();

	return e;
}

static void agh_xmpp_caps_base_entities_free(gpointer data) {
	struct agh_xmpp_caps_base_entity *b = data;

	g_free(b->name);
	g_free(b->cat);
	g_free(b->type);
	g_free(b->lang);
	g_free(b);
}

void agh_xmpp_caps_entity_dealloc(struct agh_xmpp_caps_entity *e) {

	if (!e) {
		agh_log_xmppcaps_crit("not deallocating NULL agh_xmpp_caps_entity struct");
		return;
	}

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

gint agh_xmpp_caps_add_entity(struct agh_xmpp_caps_entity *e) {
	struct agh_xmpp_caps_base_entity *b;

	if (!e) {
		agh_log_xmppcaps_crit("NULL agh_xmpp_caps_entity");
		return -1;
	}

	b = g_try_malloc0(sizeof(*b));
	if (!b) {
		agh_log_xmppcaps_crit("unable to allocate struct agh_xmpp_caps_base_entity");
		return -1;
	}

	g_queue_push_tail(e->base_entities, b);

	return (g_queue_get_length(e->base_entities)-1);
}

gint agh_xmpp_caps_set_entity_data(struct agh_xmpp_caps_entity *e, gint id, gchar *name, gchar *type, gchar *cat, gchar *lang) {
	struct agh_xmpp_caps_base_entity *b;
	gint retval;

	retval = 0;

	if (!name || !type || !cat) {
		agh_log_xmppcaps_crit("one or more mandatory argument(s) where NULL");
		retval = 20;
		goto out;
	}

	if ((!e) || (!e->base_entities)) {
		agh_log_xmppcaps_crit("passed agh_xmpp_caps_entity structure was NULL or had no base entities GQueue");
		retval = 21;
		goto out;
	}

	if (id < 0) {
		agh_log_xmppcaps_crit("id was less than 0");
		retval = 22;
		goto out;
	}

	b = g_queue_peek_nth(e->base_entities, id);
	if (!b) {
		agh_log_xmppcaps_crit("seems picked base entity is NULL");
		retval = 23;
		goto out;
	}

	b->name = g_strdup(name);

	b->type = g_strdup(type);

	b->cat = g_strdup(cat);

	if (lang)
		b->lang = g_strdup(lang);

out:
	return retval;
}

static void agh_xmpp_caps_base_entity_to_string(gpointer data, gpointer user_data) {
	struct agh_xmpp_caps_base_entity *b = data;
	GQueue *dest = user_data;
	GString *tmp;

	if (!b || !dest) {
		agh_log_xmppcaps_crit("base entity struct or dest GQueue are NULL");
		return;
	}

	tmp = g_string_new(NULL);

	g_string_append_printf(tmp, "%s/%s/%s/%s<",b->cat, b->type, b->lang ? b->lang : "", b->name);

	g_queue_push_tail(dest, g_string_free(tmp, FALSE));

	return;
}

/*
 * Only to make GCC 8+ happy. And in any case, stay on the safe side for now, avoiding to play with function casts.
 * I am pretty sure there is a GLib function we can use instead of this one.
*/
static gint agh_xmpp_caps_gcmp0_wrapper(gconstpointer a, gconstpointer b, gpointer user_data) {
	const gchar *f = a;
	const gchar *g = b;

	return g_strcmp0(f, g);
}

static gchar *agh_xmpp_caps_build_string(struct agh_xmpp_caps_entity *e) {
	gchar *tmp_str;
	GString *o;
	GQueue *entities_str;
	GQueue *features_str;
	guint i;
	guint num_elems;

	if ((!e) || (!e->base_entities)) {
		agh_log_xmppcaps_crit("agh_xmpp_caps_entity struct or base entities GQueue where missing");
		return NULL;
	}

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

	return g_string_free(o, FALSE);
}

gint agh_xmpp_caps_add_feature(struct agh_xmpp_caps_entity *e, gchar *ftext) {
	if ((!e) || (!e->features) || (!ftext)) {
		agh_log_xmppcaps_crit("features GQueue, agh_xmpp_caps_entity struct or ftext was NULL");
		return -1;
	}

	g_queue_push_tail(e->features, g_strdup(ftext));

	return (g_queue_get_length(e->features)-1);
}

gint agh_xmpp_caps_add_hash(xmpp_ctx_t *ctx, struct agh_xmpp_caps_entity *e, xmpp_stanza_t *pres) {
	xmpp_stanza_t *caps = NULL;
	gchar *hash;
	gchar *str;

	if (!e || !pres) {
		agh_log_xmppcaps_crit("agh_xmpp_caps_entity or XMPP stanza where NULL");
		return 7;
	}

	str = agh_xmpp_caps_build_string(e);

	if (!str)
		return 8;

	hash = agh_xmpp_caps_sha1(str);

	if (!hash) {
		g_free(str);
		return 9;
	}

	caps = xmpp_stanza_new(ctx);
	if (!caps) {
		agh_log_xmppcaps_crit("XMPP stanza allocation failure");
		g_free(str);
		g_free(hash);
		return 10;
	}

	xmpp_stanza_set_name(caps, AGH_XMPP_STANZA_NAME_C);
	xmpp_stanza_set_ns(caps, AGH_XMPP_STANZA_NS_CAPS);
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_HASH, "sha-1");
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_NODE, "http://meizo.net");
	xmpp_stanza_set_attribute(caps, AGH_XMPP_STANZA_ATTR_VER, hash);
	xmpp_stanza_add_child(pres, caps);
	xmpp_stanza_release(caps);
	g_free(hash);
	g_free(str);
	return 0;
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
	feature = NULL;

	if ((!xstate) || (!xstate->e) || (!xstate->e->base_entities)) {
		agh_log_xmppcaps_crit("No AGH xMPP state, or needed pointers missing in state");
		goto ffail;
	}

	capsdata = xmpp_stanza_new(xstate->xmpp_ctx);
	if (!capsdata) {
		agh_log_xmppcaps_crit("capsdata XMPP stanza allocation failure");
		goto ffail;
	}

	xmpp_stanza_set_name(capsdata, AGH_XMPP_STANZA_NAME_QUERY);

	xmpp_stanza_set_ns(capsdata, XMPP_NS_DISCO_INFO);

	num_elems = g_queue_get_length(xstate->e->base_entities);

	for (i=0;i<num_elems;i++) {
		identity = xmpp_stanza_new(xstate->xmpp_ctx);
		if (!identity) {
			agh_log_xmppcaps_crit("fragment XMPP stanza allocation failure");
			goto ffail;
		}

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
		if (!feature) {
			agh_log_xmppcaps_crit("fragment XMPP stanza allocation failure (features)");
			goto ffail;
		}

		xmpp_stanza_set_name(feature, AGH_XMPP_STANZA_NAME_FEATURE);
		ftext = g_queue_peek_nth(xstate->e->features, i);
		xmpp_stanza_set_attribute(feature, AGH_XMPP_STANZA_ATTR_VAR, ftext);
		xmpp_stanza_add_child(capsdata, feature);
		xmpp_stanza_release(feature);
	}

	return capsdata;
ffail:
	xmpp_stanza_release(capsdata);
	xmpp_stanza_release(identity);
	xmpp_stanza_release(feature);
	return NULL;
}
