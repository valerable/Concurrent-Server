/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef SERVER_H
#define SERVER_H

#include "client_registry.h"

/*
 * Client registry that should be used to track the set of
 * file descriptors of currently connected clients.
 */
extern CLIENT_REGISTRY *client_registry;

/*
 * Thread function for the thread that handles client requests.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 */
void *xacto_client_service(void *arg);

#endif
