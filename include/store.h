/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef STORE_H
#define STORE_H

/*
 * A transactional object store is a hash table that maps keys to values.
 * Both keys and values consist of arbitrary data, represented as "blobs".
 * At any given time, there can be any number of transactions involved in
 * concurrently executing operations on the store.  In order to manage this
 * concurrency, the store maintains, for each key, a list of "versions",
 * which represent possible values associated with that key.  The reason why
 * multiple versions are needed is because we don't know in advance which
 * transactions are going to access which keys, or whether more two transactions
 * will concurrently try to store conflicting values for the same key.
 * The version list associated with a key keeps track of the values seen by
 * all pending transactions. Each version consists of a "creator" transaction,
 * a data object, and a pointer to the next version in the list of versions for
 * the same key. The version list for each key is kept sorted by the creator
 * transaction ID, with versions earlier in the list having smaller creator IDs
 * than versions later in the list.
 *
 * A "committed version" is a version whose creator has committed.
 * Similarly, an "aborted version" is a version whose creator has aborted, and a
 * "pending version" is a version whose creator is still pending.
 * In a version list any committed versions always occur first, followed by
 * pending and aborted versions.  The committed version whose creator has the
 * greatest transaction ID represents the "current value" associated with the key.
 * Versions associated with creators that are still pending may eventually become
 * committed versions, if their creators commit.  Versions associated with creators
 * that have aborted will eventually be removed.  A given transaction may appear at
 * most once as a creator within the version list associated with any given key.
 *
 * When a transaction performs a GET or PUT operation for a particular key,
 * there is an initial "garbage collection" pass that is made over the version list.
 * In the garbage collection pass, all except the most recent committed version
 * is removed.  In addition, if any aborted version exists, then it and all later
 * versions in the list are removed and their creators are aborted.
 * After garbage collection, a version list will consist of at most one committed
 * version, followed by some number of pending versions.  Note that if there was
 * at least one committed version before garbage collection, there will be exactly
 * one committed version afterwards.  Also, if there were only aborted versions
 * before garbage collection, then the version list will be empty afterwards.
 *
 * After garbage collection, a GET or a PUT operation is only permitted to succeed
 * if the transaction ID of the performing transaction is greater than or equal to the
 * transaction ID of the creator of any existing version.
 * If this is not the case, then the operation has no effect and the performing transaction
 * is aborted.  If the transaction ID of the performing transaction is indeed the greatest,
 * then a new version is created, and that version is either added to the end of
 * the version list (if the transaction ID of the performing transaction is strictly
 * greater than the existing transaction IDs), or else it replaces the last version
 * in the list (if the performing transaction was in fact the creator of that version).
 * In addition, the performing transaction is set to be "dependent" on the creators
 * of any pending versions earlier in the list.  If the creators of any of those pending
 * versions subsequently abort, then a transaction dependent on them will also have
 * to abort.
 *
 * The value set in a new version created by PUT is the value specified as the argument
 * to PUT.  The value set in a new version created by GET is the same as the value of the
 * immediately preceding version (if any) in the version list, otherwise NULL (if there
 * is no immediately preceding version).  These rules ensure that transactions "read"
 * values left by transactions with smaller transaction IDs and they "write" values for
 * transactions with greater transaction IDs.  In database jargon, the sequence of
 * operations performed by transactions is "serializable" using the ordering of the
 * transaction IDs as the serialization order.
 */

#include "data.h"
#include "transaction.h"

/*
 * The overall structure of the store is that of a linked hash map.
 * A "bucket" is a singly linked list of "map entries", where each map entry
 * contains the version list associated with a single key.
 *
 * The following defines the number of buckets in the map.
 * To make things easy and simple, we will not change the number of buckets once the
 * map has been initialized, but in a more realistic implementation the number of
 * buckets would be increased if the "load factor" (average number of entries per
 * bucket) gets too high.
 */
#define NUM_BUCKETS 8

/*
 * A map entry represents one entry in the map.
 * Each map entry contains associated key, a singly linked list of versions,
 * and a pointer to the next entry in the same bucket.
 */
typedef struct map_entry {
    KEY *key;
    VERSION *versions;
    struct map_entry *next;
} MAP_ENTRY;

/*
 * The map is an array of buckets.
 * Each bucket is a singly linked list of map entries whose keys all hash
 * to the same location.
 */
struct map {
    MAP_ENTRY **table;      // The hash table.
    int num_buckets;        // Size of the table.
    pthread_mutex_t mutex;  // Mutex to protect the table.
} the_map;

/*
 * Initialize the store.
 */
void store_init(void);

/*
 * Finalize the store.
 */
void store_fini(void);

/*
 * Put a key/value mapping in the store.  The key must not be NULL.
 * The value may be NULL, in which case this operation amounts to
 * deleting any existing mapping for the given key.
 *
 * This operation inherits the key and consumes one reference on
 * the value.
 *
 * @param tp  The transaction in which the operation is being performed.
 * @param key  The key.
 * @param value  The value.
 * @return  Updated status of the transation, either TRANS_PENDING,
 *   or TRANS_ABORTED.  The purpose is to be able to avoid doing further
 *   operations in an already aborted transaction.
 */
TRANS_STATUS store_put(TRANSACTION *tp, KEY *key, BLOB *value);

/*
 * Get the value associated with a specified key.  A pointer to the
 * associated value is stored in the specified variable.
 *
 * This operation inherits the key.  The caller is responsible for
 * one reference on any returned value.
 *
 * @param tp  The transaction in which the operation is being performed.
 * @param key  The key.
 * @param valuep  A variable into which a returned value pointer may be
 *   stored.  The value pointer store may be NULL, indicating that there
 *   is no value currently associated in the store with the specified key.
 * @return  Updated status of the transation, either TRANS_PENDING,
 *   or TRANS_ABORTED.  The purpose is to be able to avoid doing further
 *   operations in an already aborted transaction.
 */
TRANS_STATUS store_get(TRANSACTION *tp, KEY *key, BLOB **valuep);

/*
 * Print the contents of the store to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void store_show(void);

#endif
