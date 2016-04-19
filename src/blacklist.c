#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <netinet/ether.h>
#include "blacklist.h"

struct mac {
	uint8_t data[ETH_ALEN];
};

struct blacklist {
	struct mac entry;
	struct blacklist *next;
};

struct blacklist *blacklist_new() {
	return calloc(sizeof(struct blacklist), 1);
}

struct mac *mac_from_string(const char *value) {
	struct mac *result = calloc(sizeof(struct mac), 1);
	if (NULL == result)
		return NULL;
	if (6 != sscanf(value, "%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 "",
		&result->data[0], &result->data[1],
		&result->data[2], &result->data[3],
		&result->data[4], &result->data[5])) {

		free(result);
		return NULL;
	}
	return result;
}

struct mac *mac_from_data(const void *value, size_t len) {
	if (len != ETH_ALEN)
		return NULL;

	struct mac *result = calloc(sizeof(struct mac), 1);
	if (NULL == result)
		return NULL;
	memcpy(result->data, value, sizeof(result->data));
	return result;
}

bool blacklist_is_empty(const struct blacklist *blacklist) {
	return NULL == blacklist->next;
}

void blacklist_append(struct blacklist *blacklist, const struct mac *mac) {
	while (true) {
		if (NULL == blacklist->next)
			break;
		blacklist = blacklist->next;
	}
	blacklist->entry = *mac;
	blacklist->next = blacklist_new();
}

static inline bool mac_equals(const struct mac *mac1, const struct mac *mac2) {
	return (0 == memcmp((const void *)mac1, (const void *)mac2, sizeof(struct mac)));
}

bool blacklist_match(const struct blacklist *blacklist, const struct mac *mac) {
	while (true) {
		if (NULL == blacklist->next)
			break;

		if (mac_equals(mac, &blacklist->entry)) {
			return true;
		}

		blacklist = blacklist->next;
	}

	return false;
}

size_t blacklist_size(struct blacklist *blacklist) {
	size_t result = 0;

	while (true) {
		if (NULL == blacklist->next)
			break;

		result++;
		blacklist = blacklist->next;
	}

	return result;
}
