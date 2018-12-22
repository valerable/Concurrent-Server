/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <pthread.h>
#include <semaphore.h>

/*
 * A transaction is a context within which to perform a sequence of operations
 * on a transactional store.  Every transaction has a unique transaction ID.
 * There is an ordering on transaction IDs, so that it makes sense to say that
 * one transaction ID is "less than" another.
 *
 * When it is first created, a transaction is in the "pending" state.
 * Each time an operation is performed in the context of a transaction,
 * there is a possibility that the transaction will enter the "aborted" state,
 * due to a conflict with other transactions executing concurrently with it.
 * Once a transaction has aborted, it is not possible to perform any further operations
 * within it and all effects that it has had on the store are deleted.  Once all the
 * desired operations have been performed within a transaction, an attempt can be made
 * to "commit" the transaction.  The transaction will then either abort or enter
 * the "committed" state.  Once a transaction has entered the committed state,
 * all effects that it has had on the store are made permanent.
 */

/*
 * Transaction status.
 * Also used as return values from transaction operations.
 */
typedef enum { TRANS_PENDING, TRANS_COMMITTED, TRANS_ABORTED } TRANS_STATUS;

/*
 * A transaction may become "dependent" on another transaction.
 * This occurs when a transaction accesses an entry in the store that has
 * previously been accessed by a pending transaction having a smaller transaction
 * ID.  Once that occurs, the dependent transaction cannot commit until the
 * the transaction on which it depends has committed.  Moreover, if the
 * other transaction aborts, then the dependent transaction must also abort.
 *
 * The dependencies of a transaction are recorded in a "dependency set",
 * which is part of the representation of a transaction.  A dependency set
 * is represented as a singly linked list of "dependency" nodes having the
 * following structure.  Any given transaction can occur at most once in a
 * single dependency set.
 */
typedef struct dependency {
  struct transaction *trans;  // Transaction on which the dependency depends.
  struct dependency *next;    // Next dependency in the set.
} DEPENDENCY;

/*
 * Structure representing a transaction.
 */
typedef struct transaction {
  unsigned int id;           // Transaction ID.
  unsigned int refcnt;       // Number of references (pointers) to transaction.
  TRANS_STATUS status;       // Current transaction status.
  DEPENDENCY *depends;       // Singly-linked list of dependencies.
  int waitcnt;               // Number of transactions waiting for this one.
  sem_t sem;                 // Semaphore to wait for transaction to commit or abort.
  pthread_mutex_t mutex;     // Mutex to protect fields.
  struct transaction *next;  // Next in list of all transactions
  struct transaction *prev;  // Prev in list of all transactions.
} TRANSACTION;

/*
 * For debugging purposes, all transactions having a nonzero reference count
 * are maintained in a circular, doubly linked list which has the following
 * sentinel as its head.
 */
TRANSACTION trans_list;

/*
 * Initialize the transaction manager.
 */
void trans_init(void);

/*
 * Finalize the transaction manager.
 */
void trans_fini(void);

/*
 * Create a new transaction.
 *
 * @return  A pointer to the new transaction (with reference count 1)
 * is returned if creation is successful, otherwise NULL is returned.
 */
TRANSACTION *trans_create(void);

/*
 * Increase the reference count on a transaction.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The transaction pointer passed as the argument.
 */
TRANSACTION *trans_ref(TRANSACTION *tp, char *why);

/*
 * Decrease the reference count on a transaction.
 * If the reference count reaches zero, the transaction is freed.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void trans_unref(TRANSACTION *tp, char *why);

/*
 * Add a transaction to the dependency set for this transaction.
 *
 * @param tp  The transaction to which the dependency is being added.
 * @param dtp  The transaction that is being added to the dependency set.
 */
void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp);

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
TRANS_STATUS trans_commit(TRANSACTION *tp);

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
TRANS_STATUS trans_abort(TRANSACTION *tp);

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
TRANS_STATUS trans_get_status(TRANSACTION *tp);

/*
 * Print information about a transaction to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 *
 * @param tp  The transaction to be shown.
 */
void trans_show(TRANSACTION *tp);

/*
 * Print information about all transactions to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void trans_show_all(void);

#endif
