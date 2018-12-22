#include "store.h"
#include "helper.h"
#include "csapp.h"
#include "debug.h"

/*
 * Initialize the store.
 */
void store_init(void){
	debug("Initialize store manager");
	//initialize the mutex
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	the_map.mutex = mutex;
	//get the number of buckets from the macro
	the_map.num_buckets = NUM_BUCKETS;
	//we need to malloc the array of map_entries
	the_map.table = calloc(the_map.num_buckets, sizeof(MAP_ENTRY *) );
}

/*
 * Finalize the store.
 */
void store_fini(void){
	//we have to remove all of the map entries and their versions
	MAP_ENTRY *index_ptr, *dump;
	for(int i = 0; i < the_map.num_buckets; i++){
		//for each bucket
		if((index_ptr = the_map.table[i])!= NULL){
			//index_ptr has the head
			while(index_ptr != NULL){
				//for each map_entry in the bucket
				dump = index_ptr;
				index_ptr = index_ptr->next;
				//we want to remove dump;
				map_entry_destroy(dump);
			}
			//all the map entries in the bucket are freed
		}
		//onto next bucket
	}
	//all buckets are freed
	//we can now free the map
	pthread_mutex_destroy(&the_map.mutex);//destroy mutex
	free(the_map.table);
}

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
TRANS_STATUS store_put(TRANSACTION *tp, KEY *key, BLOB *value){
	//get the map entry for this key, we can either find it or create it
	MAP_ENTRY *mp = find_map_entry(key);
	//Perform Grabage Collection of already commited versions
	garbage_collect(mp);
	//We got the key's map entry
	//Next, we need to add the version
	add_version(mp, tp, value);
	return trans_get_status(tp);
}

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
TRANS_STATUS store_get(TRANSACTION *tp, KEY *key, BLOB **valuep){
	//get the map entry for this key, we can either find it or create it
	MAP_ENTRY *mp = find_map_entry(key);
	//Perform Grabage Collection of already commited versions
	garbage_collect(mp);
	//this map entry has versions
	//get the lastest version
	pthread_mutex_lock(&the_map.mutex);
	VERSION *index_ptr = mp->versions;
	if(index_ptr == NULL){
		//empty versions, add a NULL blob
		*valuep = blob_create(NULL, 0);
	}
	else{
		while(index_ptr->next != NULL){
			index_ptr = index_ptr->next;
		}
		*valuep = blob_create(index_ptr->blob->content, index_ptr->blob->size);
		//make a new blob in order to prevent memory issues with the content
	}
	pthread_mutex_unlock(&the_map.mutex);
	*valuep = (add_version(mp, tp, *valuep))->blob;
	return trans_get_status(tp);
}
/*
 * Print the contents of the store to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void store_show(void){
	debug("Store show");
}