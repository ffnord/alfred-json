#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include "output.h"

static void* prepare()
{
	return NULL;
}

static void push(void *ctx __attribute__((unused)), unsigned char *id, size_t id_len, unsigned char *data, size_t data_len)
{
	uint16_t object_len;

	/* output id */

	if (id_len > UINT16_MAX)
		id_len = UINT16_MAX;

	object_len = htons(id_len);

	fwrite(&object_len, 2, 1, stdout);
	fwrite(id, id_len, 1, stdout);

	/* output data */

	if (data_len > UINT16_MAX)
		data_len = UINT16_MAX;

	object_len = htons(data_len);

	fwrite(&object_len, 2, 1, stdout);
	fwrite(data, data_len, 1, stdout);
}

static void finalize(void *ctx __attribute__((unused)))
{
}

static void cancel(void *ctx __attribute__((unused)))
{
}

const struct output_formatter output_formatter_binary = {
	.prepare = prepare,
	.push = push,
	.finalize = finalize,
	.cancel = cancel
};
