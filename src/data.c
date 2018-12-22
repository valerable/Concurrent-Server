#include <stdlib.h>
#include <pthread.h>
#include "data.h"
#include "transaction.h"
#include "string.h"
#include "helper.h"
#include "debug.h"

/*
 * Create a blob with given content and size.
 * The content is copied, rather than shared with the caller.
 * The returned blob has one reference, which becomes the caller's
 * responsibility.
 *
 * @param content  The content of the blob.
 * @param size  The size in bytes of the content.
 * @return  The new blob, which has reference count 1.
 */
BLOB *blob_create(char *content, size_t size){
	//make a new blob
	BLOB *b = malloc(sizeof(BLOB));
	//init blob
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);//initialize mutex
	b->mutex = mutex;
	pthread_mutex_lock(&b->mutex); //LOCK
	b->refcnt = 1;
	//b->content is copied from content
	if(content != NULL){
		b->size = size; //get the null terminator
		b->content = calloc(b->size, sizeof(char));
		memcpy(b->content, content, b->size);
	}
	else{
		//content is null
		b->content = NULL;
		b->size = size;
	}
	//CRITICAL CODE
	//content is copied
	b->prefix = b->content; //DEBUGGING
	pthread_mutex_unlock(&b->mutex); //UNLOCk
 	return b;
}

/*
 * Increase the reference count on a blob.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The blob pointer passed as the argument.
 */
BLOB *blob_ref(BLOB *bp, char *why){
	//use that mutex to inc the ref count
	debug("%s\n",why);
	if(bp == NULL){
		return NULL;
	}
	//LOCK
	pthread_mutex_lock(&bp->mutex);
	//CRITICAL CODE
	bp->refcnt++;
	//UNLOCK
	pthread_mutex_unlock(&bp->mutex);
	return bp;
}

/*
 * Decrease the reference count on a blob.
 * If the reference count reaches zero, the blob is freed.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void blob_unref(BLOB *bp, char *why){
	//check if null
	debug("%s\n", why);
	if(bp == NULL){
		debug("attempted to unref a NULL blob");
		return;
	}
	//LOCK
	pthread_mutex_lock(&bp->mutex);
	//CRITICAL CODE
	bp->refcnt--;
	//UNLOCK
	pthread_mutex_unlock(&bp->mutex);
	if(bp->refcnt == 0){
		pthread_mutex_lock(&bp->mutex);
		//This means that no key is referecing it, we have to free it
		//free the content
		if(bp->content != NULL){
			free(bp->content);
		}
		pthread_mutex_unlock(&bp->mutex);
		//destroy the mutex
		pthread_mutex_destroy(&bp->mutex);
		//free the blob
		free(bp);
	}
}

/*
 * Compare two blobs for equality of their content.
 *
 * @param bp1  The first blob.
 * @param bp2  The second blob.
 * @return 0 if the blobs have equal content, nonzero otherwise.
 */
int blob_compare(BLOB *bp1, BLOB *bp2){
	//take me back to when computer science was this easy ;(
	if (memcmp(bp1->content, bp2->content, bp1->size) == 0 && memcmp(bp1->content, bp2->content, bp2->size) == 0 ){
		return 0;
	}
	return -1;
}

/*
 Hash function for hashing the content of a blob.
 *
 * @param bp  The blob.
 * @return  Hash of the blob.
 */
int blob_hash(BLOB *bp){
	return hash((char *) bp->content);
}


/* Create a key from a blob.
 * The key inherits the caller's reference to the blob.
 *
 * @param bp  The blob.
 * @return  the newly created key.
 */
KEY *key_create(BLOB *bp){
	//create a key struct
	debug("key_create");
	KEY *k = malloc(sizeof(KEY));
	//put bp in key
	k->blob = bp;
	//hash
	k->hash = blob_hash(bp);
	//increment the ref count

	//blob_ref(k->blob, NULL);
	return k;
}

/*
 * Dispose of a key, decreasing the reference count of the contained blob.
 * A key must be disposed of only once and must not be referred to again
 * after it has been disposed.
 *
 * @param kp  The key.
 */
void key_dispose(KEY *kp){
	//deference the blob
	debug("key is being disposed");
	blob_unref(kp->blob, "key is unreferecing blob [key_dispose]");
	free(kp);
}

/*
 * Compare two keys for equality.
 *
 * @param kp1  The first key.
 * @param kp2  The second key.
 * @return  0 if the keys are equal, otherwise nonzero.
 */
int key_compare(KEY *kp1, KEY *kp2){
	if((blob_compare(kp1->blob, kp2->blob) == 0) && (kp1->hash == kp2->hash)){
		return 0;
	}
	return -1;
}

/*
 * Create a version of a blob for a specified creator transaction.
 * The version inherits the caller's reference to the blob.
 * The reference count of the creator transaction is increased to
 * account for the reference that is stored in the version.
 *
 * @param tp  The creator transaction.
 * @param bp  The blob.
 * @return  The newly created version.
 */
VERSION *version_create(TRANSACTION *tp, BLOB *bp){
	//create a version struct
	VERSION *v = malloc(sizeof(VERSION));
	//fill in members tp and bp
	v->creator = tp;
	v->blob = bp;
	v->next = NULL;
	v->prev = NULL;
	//increase ref counters
	if(v->creator == NULL){ //you cannot create a version that has no trans owner, unless you are the head(store.h)
		return v;
	}

	//blob_ref(v->blob, "version create blob_ref");
	trans_ref(v->creator, "version_create trans_ref");
	return v;
}

/*
 * Dispose of a version, decreasing the reference count of the
 * creator transaction and contained blob.  A version must be
 * disposed of only once and must not be referred to again once
 * it has been disposed.
 *
 * @param vp  The version to be disposed.
 */
void version_dispose(VERSION *vp){
	blob_unref(vp->blob, "version dispose blob_unref");
	trans_unref(vp->creator, "version dispose trans_unref");
	free(vp);
}