#include <jansson.h>
#include <string.h>
#include "output.h"

static void* prepare()
{
	return json_object();
}

static void push(void *ctx, unsigned char *id, size_t id_len, unsigned char *data, size_t data_len)
{
	json_t *value;
	char id_string[18];
  char *s;

	if (id_len < 6) {
		fprintf(stderr, "id too short: %zi\n", id_len);
		return;
	}

	sprintf(id_string, "%02x:%02x:%02x:%02x:%02x:%02x",
			id[0], id[1],
			id[2], id[3],
			id[4], id[5]);

	s = strndup((char *)data, data_len);
	value = json_string(s);
	free(s);

	if (value == NULL)
		fprintf(stderr, "Warning: invalid UTF-8 string from %s\n", id);
	else
		json_object_set_new(ctx, id_string, value);
}

static void finalize(void *ctx)
{
	json_dumpf(ctx, stdout, JSON_INDENT(2));
	json_decref(ctx);
}

static void cancel(void *ctx)
{
	json_decref(ctx);
}

const struct output_formatter output_formatter_string = {
	.prepare = prepare,
	.push = push,
	.finalize = finalize,
	.cancel = cancel
};
