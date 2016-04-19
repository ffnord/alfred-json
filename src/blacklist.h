#pragma once

#include <stdbool.h>

struct blacklist;
struct mac;

struct mac *mac_from_string(const char *value);
struct mac *mac_from_data(const void *value, size_t len);
struct blacklist *blacklist_new();
size_t blacklist_size(struct blacklist *blacklist);
void blacklist_append(struct blacklist*, const struct mac *mac);
bool blacklist_match(const struct blacklist *blacklist, const struct mac *mac);
