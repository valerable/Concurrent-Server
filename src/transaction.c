#include "transaction.h"
#include "helper.h"
#include "csapp.h"
#include "debug.h"


/*
 * Initialize the transaction manager.
 */
void trans_init(void){
	debug("Initialize transaction manager");
	//set up the sentinal head
	trans_list.id = 0; //the head has the 0 id
	trans_list.next = &trans_list;
	trans_list.prev = &trans_list;
}

/*
 * Finalize the transaction manager.
 */
void trans_fini(void){
	//Iterate through our LL of transactions and free each transaction
	TRANSACTION *dump_ptr = &trans_list;
	TRANSACTION *current_ptr = dump_ptr->next;
	while(current_ptr != &trans_list){
		dump_ptr = current_ptr;
		current_ptr = dump_ptr->next;
		trans_unref(dump_ptr, "trans_unref from [trans_fini]");
	}
}

/*
 * Create a new transaction.
 *
 * @return  A pointer to the new transaction (with reference count 1)
 * is returned if creation is successful, otherwise NULL is returned.
 */
TRANSACTION *trans_create(void){
	TRANSACTION *t = malloc(sizeof(TRANSACTION));
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);//initialize mutex
	sem_t sem;
	sem_init(&sem, 0, 0); //initialize sem = 0
	// DEPENDENCY *depends_list = malloc(sizeof(DEPENDENCY)); //SENTINEL HEAD FOR DEPENDENCIES
	// depends_list->trans = NULL;
	// depends_list->next = NULL;
	//Add to field members
	t->refcnt = 1;
	t->status = TRANS_PENDING;
	t->depends = NULL;
	t->waitcnt = 0;
	t->sem = sem;
	t->mutex = mutex;
	//LOCK
	pthread_mutex_lock(&t->mutex);
	//CRITICAL CODE
	t->id = (trans_list.id);
	trans_list.id++;
	add_transaction_to_LL(t);
	//UNLOCK
	pthread_mutex_unlock(&t->mutex);
	return t;
}

/*
 * Increase the reference count on a transaction.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The transaction pointer passed as the argument.
 */
TRANSACTION *trans_ref(TRANSACTION *tp, char *why){
	debug("%s\n",why);
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	tp->refcnt++;
	//UNLOCK
	pthread_mutex_unlock(&tp->mutex);
	return tp;
}

/*
 * Decrease the reference count on a transaction.
 * If the reference count reaches zero, the transaction is freed.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void trans_unref(TRANSACTION *tp, char *why){
	debug("%s\n",why);
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	tp->refcnt--;
	//UNLOCK
	if(tp->refcnt == 0){
		pthread_mutex_unlock(&tp->mutex);
		//destroy the transaction
		trans_destroy(tp);
		return;
	}
	pthread_mutex_unlock(&tp->mutex);
}

/*
 * Add a transaction to the dependency set for this transaction.
 *
 * @param tp  The transaction to which the dependency is being added.
 * @param dtp  The transaction that is being added to the dependency set.
 */
void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp){
	//create new dependecy
	DEPENDENCY *d = malloc(sizeof(DEPENDENCY));
	d->trans = tp;
	d->next = NULL;
	//dependecy has been created
	//add_ref to the dtp
	trans_ref(dtp, "add_dependency");
	//add it to our depends list
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	DEPENDENCY *index_ptr = dtp->depends; //first one is always the head
	if(index_ptr == NULL){
		//there are 0 depends
		dtp->depends = d;
	}
	else {
		while(index_ptr != NULL){
			//reached a dependecy
			index_ptr = index_ptr->next; //get the next
		}
		//at this point, index_ptr should be last dependency
		index_ptr->next = d; //append to the LL
	}
	tp->waitcnt++; //dependent transaction waiting for this one
	//UNLOCK
	pthread_mutex_unlock(&tp->mutex);

}


/*
 * Try to commit a transaction.  Committing a transaction requires waiting
 * for all transactions in its dependency set to either commit or abort.
 * If any transaction in the dependency set abort, then the dependent
 * transaction must also abort.  If all transactions in the dependency set
 * commit, then the dependent transaction may also commit.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be committed.
 * @return  The final status of the transaction: either TRANS_ABORTED,
 * or TRANS_COMMITTED.
 */
TRANS_STATUS trans_commit(TRANSACTION *tp){
	TRANS_STATUS return_status;
	while(tp->waitcnt){	//for the amount of dependencies this transaction is apart of
		P(&tp->sem); //wait for another transaction from the list of depends to commit or abort
		//another trans has unlocked us with a V message
		pthread_mutex_lock(&tp->mutex);
		tp->waitcnt--;
		pthread_mutex_unlock(&tp->mutex);
		//check if we aborted
		if(trans_get_status(tp) == TRANS_ABORTED){
			//we aborted, which means our depends list must abort too
			return_status = trans_abort(tp);
			//trans_unref(tp, "trans_commit"); MAYBE?????
			return return_status;//return that we aborted
		}
		//we didn't abort, so the message was probably a commit
	}
	//Done waiting for other transations
	//if we made it here, we didn't abort and all of transactions we depend on commited

	//we must commit
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	tp->status = TRANS_COMMITTED;
	//UNLOCK
	pthread_mutex_unlock(&tp->mutex);
	//we should let everyone in our depends list that we commited
	DEPENDENCY *index_ptr = tp->depends;//get the first dependency
	while(index_ptr != NULL){
		V(&index_ptr->trans->sem);//ALERT THE TRANSACTIONS SEMAPHORE TO WAKE UP
		index_ptr = index_ptr->next; //Onto the next dependency
	}
	//consume a single reference
	return_status = trans_get_status(tp);
	trans_unref(tp, "trans unref from [trans_commit]");
	return return_status;
}

/*
 * Abort a transaction.  If the transaction has already committed, it is
 * a fatal error and the program crashes.  If the transaction has already
 * aborted, no change is made to its state.  If the transaction is pending,
 * then it is set to the aborted state, and any transactions dependent on
 * this transaction must also abort.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be aborted.
 * @return  TRANS_ABORTED.
 */
TRANS_STATUS trans_abort(TRANSACTION *tp){
	TRANS_STATUS return_status = trans_get_status(tp);
	if(return_status == TRANS_ABORTED){
		//already aborted
	}
	else {
		//Set the transaction to aborted
		//LOCK
		pthread_mutex_lock(&tp->mutex);
		//CRITICAL CODE
		tp->status = TRANS_ABORTED;
		//UNLOCK
		pthread_mutex_unlock(&tp->mutex);
	}
	//alert tp's dependency list that an abort occured
	DEPENDENCY *index_ptr = tp->depends;//get the first depends
	while(index_ptr != NULL){ //while its not null
		if(trans_get_status(index_ptr->trans) == TRANS_PENDING){//For each transaction that is pending
			//Change the trans status to abort
			pthread_mutex_lock(&index_ptr->trans->mutex);
			index_ptr->trans->status = TRANS_ABORTED; //SET THE DEPENDENT TO ABORTED
			pthread_mutex_unlock(&index_ptr->trans->mutex);
			V(&index_ptr->trans->sem);//ALERT THE TRANSACTIONS SEMAPHORE TO WAKE UP
		}
		index_ptr = index_ptr->next; //Onto the next dependency

	}
	//We have set the transaction to aborted
	//consume a single reference
	return_status = trans_get_status(tp);
	trans_unref(tp, "trans_abort");
	return return_status;
}

/*
 * Get the current status of a transaction.
 * If the value returned is TRANS_PENDING, then we learn nothing,
 * because unless we are holding the transaction mutex the transaction
 * could be aborted at any time.  However, if the value returned is
 * either TRANS_COMMITTED or TRANS_ABORTED, then that value is the
 * stable final status of the transaction.
 *
 * @param tp  The transaction.
 * @return  The status of the transaction, as it was at the time of call.
 */
TRANS_STATUS trans_get_status(TRANSACTION *tp){
	TRANS_STATUS current_status;
	//LOCK
	pthread_mutex_lock(&tp->mutex);
	//CRITICAL CODE
	current_status = tp->status;
	//UNLOCK
	pthread_mutex_unlock(&tp->mutex);
	return current_status;
}

/*
 * Print information about a transaction to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 *
 * @param tp  The transaction to be shown.
 */
void trans_show(TRANSACTION *tp){
	//IDK WHAT TO DO
	debug("trans show");
}

/*
 * Print information about all transactions to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void trans_show_all(void){
	//IDK WHAT TO DO
	debug("trans show all");
}
