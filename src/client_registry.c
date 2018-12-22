
#include "client_registry.h"
#include "csapp.h"
#include "debug.h"

typedef struct client_registry {
	int client_count;		//number of fds
	fd_set client_fds;		//our set of fds
	pthread_mutex_t mutex;	//Mutex(true/false)
	sem_t wait_sem;			//Semaphore(sync counter)
} CLIENT_REGISTRY;

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry.
 */
CLIENT_REGISTRY *creg_init(){
	//create the client registry
	debug("Initialize client registry");
	CLIENT_REGISTRY *cr = malloc(sizeof(CLIENT_REGISTRY));
	fd_set client_fds;
	pthread_mutex_t mutex;
	sem_t wait_sem;
	//memset
	memset(cr, 0, sizeof(CLIENT_REGISTRY));
	//set the fields
	cr->client_count = 0; //number of fds
	FD_ZERO(&client_fds);//initialize fdset
	cr->client_fds = client_fds; //file descriptor set
	pthread_mutex_init(&mutex, NULL);//initialize mutex
	cr->mutex = mutex;
	sem_init(&wait_sem, 0, 0); //sem = 0
	cr->wait_sem = wait_sem;
	return cr;
}

/*
 * Finalize a client registry.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr){
	debug("Finalize client registry");
	pthread_mutex_destroy(&(cr->mutex));
	sem_destroy(&(cr->wait_sem));
	free(cr);
}

/*
 * Register a client file descriptor.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 */
void creg_register(CLIENT_REGISTRY *cr, int fd){
	//we should add fd to our fdset and inc the number of client counts
	//client count and client fds are shared, we need to lock them per process
	//LOCK
	pthread_mutex_lock(&cr->mutex);
	//CRITICAL CODE
	FD_SET(fd, &(cr->client_fds)); //ad fd to the set
	cr->client_count++; //inc the number of reg clients
	//UNLOCK
	pthread_mutex_unlock(&cr->mutex);
}


/* Unregister a client file descriptor, alerting anybody waiting
 * for the registered set to become empty.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be unregistered.
 */
void creg_unregister(CLIENT_REGISTRY *cr, int fd){
	//we should remove the fd form our fset and dec the number of clients
	//client count and client fds are shared, we need to lock them per process
	//LOCK
	pthread_mutex_lock(&cr->mutex);
	//CRITICAL CODE
	FD_CLR(fd, &(cr->client_fds));
	cr->client_count--;
	//UNLOCK
	pthread_mutex_unlock(&cr->mutex);
	if(cr->client_count == 0){
		V(&(cr->wait_sem)); //ALERT
	}

}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
	//An invocation of this functions incs the number of threads waiting
	P(&(cr->wait_sem)); //HANG until we get an alert from V
	//P returned, which means a fd was unregistred

}

/*
 * Shut down all the currently registered client file descriptors.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr){
	//we need to unregister all of our connected clients
	//we will iterate through our fd set
	debug("my creg_shutdown_all");
	for(int i = 0; i < FD_SETSIZE; i++){
		if(FD_ISSET(i, &(cr->client_fds))){
			//we found a set fd
			//Shut it down
			shutdown(i, SHUT_RD);
		}
	}
	V(&(cr->wait_sem)); //ALERT

}


