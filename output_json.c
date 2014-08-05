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
	json_error_t error;
	char id_string[18];

	if (id_len < 6) {
		fprintf(stderr, "id too short: %zi\n", id_len);
		return;
	}

	sprintf(id_string, "%02x:%02x:%02x:%02x:%02x:%02x",
			id[0], id[1],
			id[2], id[3],
			id[4], id[5]);

	value = json_loadb((char *)data, data_len, 0, &error);

	if (value == NULL)
		fprintf(stderr, "Warning: invalid JSON object from %s: %s\n", id_string, error.text);
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

const struct output_formatter output_formatter_json = {
	.prepare = prepare,
	.push = push,
	.finalize = finalize,
	.cancel = cancel
};
