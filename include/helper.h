/*
 *header file for my helper functions
 */
#include "data.h"
#include "transaction.h"
#include "csapp.h"
#include "store.h"
int string_to_int(char *string);
unsigned long hash(void *str);
void add_transaction_to_LL(TRANSACTION *z);
void remove_transaction_from_LL(TRANSACTION *z);
void trans_destroy(TRANSACTION *tp);
MAP_ENTRY *map_entry_create(KEY *kp);
void map_entry_destroy(MAP_ENTRY *mp);
MAP_ENTRY *find_map_entry(KEY *kp);
VERSION *add_version(MAP_ENTRY *mp, TRANSACTION *tp, BLOB *value);
void garbage_collect(MAP_ENTRY *mp);
VERSION *remove_version_from_LL(VERSION *vp, VERSION *head);