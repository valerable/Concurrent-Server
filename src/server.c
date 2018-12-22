#include "server.h"
#include "transaction.h"
#include "csapp.h"
#include "protocol.h"
#include "data.h"
#include "store.h"


CLIENT_REGISTRY *client_registry;
/*
 * Thread function for the thread that handles client requests.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 */
void *xacto_client_service(void *arg){
	//arg holds the connfd
	//get our local copy of the connfd
	int connfd = *((int *)arg);
	free(arg);
	//detach the thread
	Pthread_detach(pthread_self());
	//register the connfd to the client registry
	creg_register(client_registry, connfd);
	//create transaction for this specific session
	TRANSACTION *tp = trans_create();
	//thread enters the service loop
	XACTO_PACKET pkt;
	memset(&pkt, 0, sizeof(XACTO_PACKET));
	void **datap = malloc(sizeof(void *));
	BLOB **valuep = malloc(sizeof(BLOB *));
	//*valuep = malloc(sizeof(BLOB));
	struct timespec current_time;
	BLOB *key_blob;
	KEY *key;
	BLOB *value;
	TRANS_STATUS current_status = trans_get_status(tp);
	while(current_status == TRANS_PENDING){//while true
		//recieve a request packet sent by the client
	if(proto_recv_packet(connfd, &pkt, NULL) == 0){//packet recieved success
			//determine it header or payload
			//if the payload is null, it is a header packet
			switch(pkt.type){
				///////////////////
				case XACTO_PUT_PKT:
				///////////////////
				//Handle PUT
				//clean datap
				memset(datap, 0, sizeof(void *)); //clean the buffer
				memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
				if(proto_recv_packet(connfd, &pkt, datap) == -1){//packet
					//Unexpected EOF
					current_status = trans_abort(tp);
					break;
				}
				//got the key
				//perform blob operation
				key_blob = blob_create(*datap, pkt.size);
				key = key_create(key_blob);
				free(*datap);
				//create key to this content
				//we have our key
				memset(datap, 0, sizeof(void *)); //clean the buffer
				memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
				if(proto_recv_packet(connfd, &pkt, datap) == -1){//packet
					//Unexpected EOF
					current_status = trans_abort(tp);
					break;
				}
				//got the value
				//perform operations
				value = blob_create(*datap, pkt.size);
				free(*datap);
				//we now have the key and the value
				current_status = store_put(tp, key, value);//add our key and value to the hash map
				if(current_status == TRANS_ABORTED){
					break;
				}
				//we have to reply to the client
				memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
				//SEND REPLY HEADER
				clock_gettime(CLOCK_REALTIME, &current_time);
				pkt.type = XACTO_REPLY_PKT;
				pkt.status = 0;
				pkt.size = 0;//payload size of this header is 0
				pkt.timestamp_sec = current_time.tv_sec;
				pkt.timestamp_nsec = current_time.tv_nsec;
				if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
					//Unexpected EOF
					current_status = trans_abort(tp);
					break;
				}
				break;
				///////////////////
				case XACTO_GET_PKT:
				///////////////////
				//Handle GET
				//clean datap
				memset(datap, 0, sizeof(void *)); //clean the buffer
				memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
				memset(valuep, 0, sizeof(BLOB *)); //clean the buffer
				if(proto_recv_packet(connfd, &pkt, datap) == -1){//packet
					//Unexpected EOF
					current_status = trans_abort(tp);
					break;
				}
				//got the key
				//perform blob operation
				key_blob = blob_create(*datap, pkt.size);
				free(*datap);
				//create key to this content
				key = key_create(key_blob);
				//we have our key
				//perform operations
				current_status = store_get(tp, key, valuep);//get the value based on the key and store it to the valuep buffer
				if(current_status == TRANS_ABORTED){
					break;
				}
				//BLOB **valuep, remember that
				//we have to reply to the client
				//check if the valuep is NULL(could be null)
				if((*valuep)->content){
					//NOT NULL content
					memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
					//SEND REPLY HEADER
					clock_gettime(CLOCK_REALTIME, &current_time);
					pkt.type = XACTO_REPLY_PKT;
					pkt.status = 0;
					pkt.size = 0;//payload size of this header is 0
					pkt.timestamp_sec = current_time.tv_sec;
					pkt.timestamp_nsec = current_time.tv_nsec;
					if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
						//Unexpected EOF
						current_status = trans_abort(tp);
						break;
					}
					//SEND DATA PACKET
					memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
					clock_gettime(CLOCK_REALTIME, &current_time);
					pkt.type = XACTO_DATA_PKT;
					pkt.status = 0;
					pkt.size = (*valuep)->size;//payload size is blob size
					pkt.timestamp_sec = current_time.tv_sec;
					pkt.timestamp_nsec = current_time.tv_nsec;
					if(proto_send_packet(connfd, &pkt, (*valuep)->content) == -1){//packet
						//Unexpected EOF
						current_status = trans_abort(tp);
						break;
					}

				}
				else{
					//NULL
					memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
					//SEND REPLY HEADER
					clock_gettime(CLOCK_REALTIME, &current_time);
					pkt.type = XACTO_REPLY_PKT;
					pkt.status = 0;
					pkt.size = 0;//payload size of this header is 0
					pkt.timestamp_sec = current_time.tv_sec;
					pkt.timestamp_nsec = current_time.tv_nsec;
					if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
						//Unexpected EOF
						current_status = trans_abort(tp);
						break;
					}
					//SEND DATA PACKET
					memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
					clock_gettime(CLOCK_REALTIME, &current_time);
					pkt.type = XACTO_DATA_PKT;
					pkt.status = 0;
					pkt.size = 0;//payload size of NULL is 0
					pkt.null = 1;//no data
					pkt.timestamp_sec = current_time.tv_sec;
					pkt.timestamp_nsec = current_time.tv_nsec;
					if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
						//Unexpected EOF
						current_status = trans_abort(tp);
						break;
					}
				}
				// blob_unref(*valuep, "blob unref the valuep from [xacto_get]");
				break;
				case XACTO_COMMIT_PKT:
				//Handle COMMIT
				//0 payload packets
				//we have to reply to the client
				current_status = trans_commit(tp);
				if(current_status == TRANS_ABORTED){
					break;
				}
				memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
				//SEND REPLY HEADER
				clock_gettime(CLOCK_REALTIME, &current_time);
				pkt.type = XACTO_REPLY_PKT;
				pkt.status = TRANS_COMMITTED;
				pkt.size = 0;//payload size of this header is 0
				pkt.timestamp_sec = current_time.tv_sec;
				pkt.timestamp_nsec = current_time.tv_nsec;
				if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
					//Unexpected EOF
					current_status = trans_abort(tp);
					break;
				}
				break;
			}
		}
		else if(proto_recv_packet(connfd, &pkt, NULL) == -1){//packet
			//Unexpected EOF
			current_status = trans_abort(tp);
			break;
		}
	}
	if(current_status == TRANS_ABORTED){
		//Send a final reply with aborted status
		memset(&pkt, 0, sizeof(XACTO_PACKET)); //clean the buffer
		//SEND REPLY HEADER
		clock_gettime(CLOCK_REALTIME, &current_time);
		pkt.type = XACTO_REPLY_PKT;
		pkt.status = TRANS_ABORTED;
		pkt.size = 0;//payload size of this header is 0
		pkt.timestamp_sec = current_time.tv_sec;
		pkt.timestamp_nsec = current_time.tv_nsec;
		if(proto_send_packet(connfd, &pkt, NULL) == -1){//packet
			//Unexpected EOF
		}
	}
	//The status changed, transaction was either commited or aborted
	//Unregister connfd
	trans_show_all();
	free(datap);
	free(valuep);
	creg_unregister(client_registry, connfd);
	close(connfd);
	return NULL;
}