/*
 *header file for my helper functions
 */
#include "helper.h"
#include "debug.h"

/*Converts a string to a positive int
 *returns -1 if the string was not a integer
 */
int string_to_int(char *string){
	int i = 0;
	int number = 0;
	while(*(string+i) != 0){ //while not reaching the Null terminiator
		if( *(string + i) >= '0' && *(string + i) <= '9') {
		//get int
			number *= 10;
			number += (int) ((char) *(string + i)) - '0';
		}
		else{
			//IF IT ever reaches here, then we have an error because the string contains a character, not int
			return -1;
		}
		i++;
	}
	return number;
}

/* Hashing function used to create a unique has int for the string
 * @param The string create the hash
 * @return The unique hash of the string
 */
unsigned long hash(void *str){
	unsigned long hash = 5381;
	int character;
	int i = 0;
	while( (character = *((char *)(str+i))) ){
		hash = ((hash << 5) + hash) + character; /* hash * 33 + c */
		i++;
	}
    return hash % the_map.num_buckets; //This returns a number between 0 and num_buckets -1
}


/*
 * Add a transaction to the Linked List
 *
 * @param  A pointer to the transaction to add
 *
 */
void add_transaction_to_LL(TRANSACTION *z){
	z->next = &trans_list;
	z->prev= trans_list.prev;
	trans_list.prev->next = z;
	trans_list.prev = z;
}

/*
 * Remove a transaction from the Linked List
 *
 * @param  A pointer to the transaction to remove
 *
 */
void remove_transaction_from_LL(TRANSACTION *z){
	z->prev->next = z->next;
	z->next->prev = z->prev;
	z->next = NULL;
	z->prev = NULL;
}

/*
 * Destroy a transaction.
 *
 * @param  A pointer to the transaction that must be removed and freed
 *
 */
void trans_destroy(TRANSACTION *tp){
	//remove from the free list
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	remove_transaction_from_LL(tp);
	//now that it has been unlinked, destroy it
	sem_destroy(&(tp->sem));
	//Iterate through the LL of dependecies and free each one
	DEPENDENCY *index_ptr = tp->depends; //tp->depends is the head
	DEPENDENCY *dump = NULL;
	while(index_ptr != NULL){
		//reached a dependecy
		dump = index_ptr;
		trans_unref(index_ptr->trans, "trans unref from [trans_destroy]");
		index_ptr = index_ptr->next; //get the next
		free(dump);//free
	}
	//UNLOCK
	//Free the mutex
	pthread_mutex_unlock(&tp->mutex);
	//free the transaction
	pthread_mutex_destroy(&(tp->mutex));
	free(tp);
}

/*
 * Create a blank map entry.(Bucket)
 *
 * @param key
 * @return a pointer to the MAP_ENTRY
 *
 */
MAP_ENTRY *map_entry_create(KEY *kp){
	//VERSION *head = version_create(NULL, NULL);//our sentinal head
	//head->next = NULL;
	MAP_ENTRY *m = malloc(sizeof(MAP_ENTRY));
	m->key = kp;
	m->versions = NULL; //LINKED LIST OF VERIONS
	m->next = NULL; //next entry from this bucket
	return m;
}

void map_entry_destroy(MAP_ENTRY *mp){
	//we have to destroy this map entry
	//we have to handle the versions
	VERSION *index_ptr = mp->versions;
	VERSION *dump;
	while(index_ptr != NULL){
		dump = index_ptr;
		index_ptr = index_ptr->next;
		version_dispose(dump);
	}
	key_dispose(mp->key);//throw away the key
	free(mp);//free it
}

/*
 * find a map entry of equal key
 * If map entry does not exist, it will add the key from input to the hashmap
 *
 * @param key
 * @return a pointer to the MAP_ENTRY
 *
 */
MAP_ENTRY *find_map_entry(KEY *kp){
	//We have to find the map entry whose key matches kp
	//get our hash map the_map.table;
	//LOCK
	MAP_ENTRY *index_ptr;
	pthread_mutex_lock(&the_map.mutex);
	//Go find a match in the table
	if(the_map.table[kp->hash] != NULL){//make sure we aren't accessing a null bucket
		//found a bucket with a key entry, iterate through that
		index_ptr = the_map.table[kp->hash];
		while(index_ptr != NULL){
			//compare the key
			if(key_compare(index_ptr->key, kp) == 0){ //THE KEYS MATCH
				//Since we found a redundant key, we need to dispose of the kp
				key_dispose(kp);
				pthread_mutex_unlock(&the_map.mutex);
				return index_ptr;
			}
			index_ptr = index_ptr->next;
		}
	}
	//If we are here, that means we didn't find a match, so add it to the table!
	//we want to add it to the bucket equal to the hash
	if(the_map.table[kp->hash] == NULL){//found a NULL bucket
		//first key of the bucket
		the_map.table[kp->hash] = map_entry_create(kp);
		pthread_mutex_unlock(&the_map.mutex);
		return the_map.table[kp->hash];
 	}
 	else{
 		//other keys in this bucket
 		index_ptr = the_map.table[kp->hash];
 		while(index_ptr->next != NULL){
 			index_ptr = index_ptr->next;
 		}
 		index_ptr->next = map_entry_create(kp);
 		pthread_mutex_unlock(&the_map.mutex);
		return index_ptr->next;
 	}
}

/*
 * we add a version of map entry
 *
 * @param map entry, transaction pointer, value
 *
 */
VERSION *add_version(MAP_ENTRY *mp, TRANSACTION *tp, BLOB *value){
	//first, we shall create the version
	//LOCK
	pthread_mutex_lock(&the_map.mutex);
	// VERSION *vp = version_create(tp, value);
	VERSION *index_ptr = mp->versions;
	VERSION *temp;
	//vp holds the version we want to create
	//check the mp if there are any previous version existed
	if(index_ptr == NULL){
		//this map entry has no versions to it
		//add the version to this map_entry
		mp->versions = version_create(tp, value);
		//a successful put!
		pthread_mutex_unlock(&the_map.mutex);
		return mp->versions;
	}
	else{
		//index_ptr is not null
		//this map entry has versions
		//get the lastest version
		while(index_ptr->next != NULL){
			index_ptr = index_ptr->next;
		}
		//index_ptr has the lastest version
		if(trans_get_status(index_ptr->creator) == TRANS_PENDING ){
			//we can append this new version to a pending version
			//check to see if the transaction ID is less than or equal to
			if(index_ptr->creator->id == tp->id){
				//versions are from same transaction, OVERWRITE IT
				temp = index_ptr;//put the old lastest version in a temp
				index_ptr = remove_version_from_LL(index_ptr, mp->versions);//get the prev
				if(index_ptr == mp->versions){//IT WAS THE HEAD
					//we were attempting to remove the head
					//overwrite at the head
					mp->versions = version_create(tp, value);//rewrite the head
					index_ptr = mp->versions;//point to the newly created
					version_dispose(temp);
					pthread_mutex_unlock(&the_map.mutex);
					return index_ptr;
				}
				else{//index_ptr is the version right before the one we want to overwrite
					index_ptr->next = version_create(tp, value);//point to the newly created
					//dispose original
					version_dispose(temp);
					pthread_mutex_unlock(&the_map.mutex);
					return index_ptr->next;
				}
			}
			//
			else {
				//transactoin id is not the same
				//APPEND IT
				if(index_ptr->creator->id > tp->id){
					//a transaction id that is less was appended
					trans_abort(tp);
					//ABORT IT
					pthread_mutex_unlock(&the_map.mutex);
					return index_ptr; //return that version
				}
				trans_add_dependency(tp, index_ptr->creator);//add the dependency
				index_ptr->next = version_create(tp, value);//append version
				//Append was successful
				pthread_mutex_unlock(&the_map.mutex);
				return index_ptr->next;

			}
		}
		else if(trans_get_status(index_ptr->creator) == TRANS_ABORTED){
			//we are attempting to add a version to a aborted transaction
			//ABORT
			trans_abort(tp); //abort this transaction
			pthread_mutex_unlock(&the_map.mutex);
			return index_ptr; //return that aborted version
		}
		else if(trans_get_status(index_ptr->creator) == TRANS_COMMITTED){
			//we can append this new verion to the commited version
			index_ptr->next = version_create(tp, value);
			//Append was successful
			pthread_mutex_unlock(&the_map.mutex);
			return index_ptr->next;
		}
		debug("Nothing happened");
		pthread_mutex_unlock(&the_map.mutex);
		return NULL;
	}
}

/*
 * collect any transactions that are already commited
 *
 * @param map entry, transaction pointer, value
 *
 */
void garbage_collect(MAP_ENTRY *mp){
	//LOCK
	pthread_mutex_lock(&the_map.mutex);
	VERSION *index_ptr = mp->versions;
	VERSION *temp;
	int recent = 0, i = 0;
	if(index_ptr == NULL){
		//UNLOCK
		pthread_mutex_unlock(&the_map.mutex);
		return;
	}
	for(i = 0; index_ptr != NULL; i++, index_ptr = index_ptr->next){
		//i gets the version[i] we are on
		if(index_ptr->creator != NULL){
			if(trans_get_status(index_ptr->creator) == TRANS_ABORTED){
				//we found a aborted version, we have to remove it and all next ones
				//remove this one
				if(index_ptr == mp->versions){//if we are removing the head node
					temp = index_ptr;
					mp->versions = index_ptr->next; //make the next one the new head
					if(mp->versions == NULL){//if the head turned NULL
						//don't call another abort
					}
					else{
						if(mp->versions->next != NULL){//head exists, so check if there is a next
							trans_abort(mp->versions->next->creator);//abort the next one
						}
					}
					version_dispose(index_ptr);//dispose of the head
				}
				else {//we are not at the head
					temp = index_ptr;
					index_ptr = remove_version_from_LL(index_ptr, mp->versions);//index_ptr gets the previous
					if(index_ptr->next != NULL){
						trans_abort(index_ptr->next->creator);//abort the next one
					}
					version_dispose(temp);//throw away this one
				}
			}
			else if(trans_get_status(index_ptr->creator) == TRANS_COMMITTED){
				//we have to find the most recent one
				recent = i;
			}
		}
	}
	//we took care of the aborts, now for the commits
	//recent has the committed we want to keep
	index_ptr = mp->versions;
	for(i = 0; i < recent; i++, index_ptr = index_ptr->next){
		if(trans_get_status(index_ptr->creator) == TRANS_COMMITTED){
			//remove it
			if(index_ptr == mp->versions){//if we are removing the firsts node
				//remove the head
				mp->versions = index_ptr->next;
				version_dispose(index_ptr);
			}
			else{
				//the node we want to remove is not the first one
				temp = index_ptr;
				index_ptr = remove_version_from_LL(index_ptr, mp->versions);
				version_dispose(temp);
			}
		}
	}
	//UNLOCK
	pthread_mutex_unlock(&the_map.mutex);
}

VERSION *remove_version_from_LL(VERSION *vp, VERSION *head){
	//start at the head
	//remove vp while fixing next pointers
	VERSION *prev_ptr = head; //get the FRONT
	while(prev_ptr != vp){
		prev_ptr = prev_ptr->next;
	}
	//prev_ptr is the version right before
	prev_ptr->next = vp->next;
	//vp is now removed
	vp->next = NULL;
	return prev_ptr;//pointer right before the deleted one
}