#include "debug.h"
#include "client_registry.h"
#include "transaction.h"
#include "store.h"
#include "csapp.h"
#include "helper.h"
#include "server.h"

static void terminate(int status);
static void sighup_handler(int status);

CLIENT_REGISTRY *client_registry;
int *connfd;

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    Signal(SIGHUP, sighup_handler); //sighup handlers here
    debug("pid: %d\n",getpid());
    if(argv[1] == NULL){
        //-p is not there
        fprintf(stderr, "Usage: %s [-p <port>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char optval;
    char *port;
    int port_checker = -1;
    while(optind < argc) {
        if((optval = getopt(argc, argv, "p:?")) != -1) {
            switch(optval) {
            case 'p':
            port_checker = string_to_int(optarg);
            if(port_checker < 0 || port_checker > 65535){
                //invalid port number
                fprintf(stderr, "invalid port argument: %s [0 - 65535]\n", optarg);
                exit(EXIT_FAILURE);
            }
            //port is valid
            port = optarg;
            break;
            case '?':
            //print Help Msg
            fprintf(stderr, "Usage: %s [-p <port>]\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
            default:

            break;
            }
        }
    }
    //port has our port number
    // Perform required initializations of the client_registry,
    // transaction manager, and object store.
    client_registry = creg_init();
    trans_init();
    store_init();
    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    listenfd = Open_listenfd(port); //file descriptor for listening(incoming connctions)
    while(1){
        pthread_t tid;
        clientlen = sizeof(struct sockaddr_storage);
        connfd = (int *) malloc(sizeof(int)); //malloc space for the connfd
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen); //connfd now has the connected file descriptor with accepted client
        if(*connfd == -1){
            break;
        }
        Pthread_create(&tid, NULL, xacto_client_service, connfd);
        //free(connfd);
    }
    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    free(connfd);
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    trans_fini();
    store_fini();

    debug("Xacto server terminating");
    exit(status);
}

void sighup_handler(int status){
    terminate(EXIT_SUCCESS);
}