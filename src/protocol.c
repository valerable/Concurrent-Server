#include "protocol.h"
#include "csapp.h"
#include "helper.h"


/*
 * Send a packet, followed by an associated data payload, if any.
 * Multi-byte fields in the packet are converted to network byte order
 * before sending.  The structure passed to this function may be modified
 * as a result of this conversion process.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param pkt  The fixed-size part of the packet, with multi-byte fields
 *   in host byte order
 * @param data  The payload for data packet, or NULL.  A NULL value used
 *   here for a data packet specifies the transmission of a special null
 *   data value, which has no content.
 * @return  0 in case of successful transmission, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 */

int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data){
	//fd is the file descriptor for communication(connfd)
	//pkt is a pointer to the HEADER of the PACKET(REPLY PKT) we will send
	//data is the payload(could be null)

	//fd is connfd to write to
	//get the size of the payload
	int bytes_written = 0, bytes_to_write = 0;
	//traverse data until NULL to get size
	//CONVERSION TO NETWORK ORDER
	pkt->size = htonl(pkt->size);
	pkt->timestamp_sec = htonl(pkt->timestamp_sec);
	pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);

	bytes_to_write = sizeof(XACTO_PACKET); //NUMBER OF BYTES TO WRITE TO FD
	while(bytes_to_write > 0){//while there are bytes to write
		if((bytes_written = write(fd, pkt, bytes_to_write)) <= 0) { //PERFORM WRITE
			//BAD WRITE
			return -1;
		}
		//write operated fine
		bytes_to_write -= bytes_written; //decrement this loop
	}
	//Header has been written
	//Check for payload
	if(pkt->size > 0){ //data was not null, we need to invoke another Write
		bytes_to_write = ntohl(pkt->size);//Get the payload size
		while(bytes_to_write > 0){//while there are bytes to write
			if((bytes_written = write(fd, data, bytes_to_write)) <= 0) { //PERFORM WRITE
				//BAD WRITE
				return -1;
			}
			//write operated fine
			bytes_to_write -= bytes_written; //decrement this loop
		}
		//payload has been written
	}
	return 0;
}

/*
 * Receive a packet, blocking until one is available.
 * The returned structure has its multi-byte fields in host byte order.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param pkt  Pointer to caller-supplied storage for the fixed-size
 *   portion of the packet.
 * @param datap  Pointer to variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * If the returned payload pointer is non-NULL, then the caller assumes
 * responsibility for freeing the storage.
 */
int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap){
	//fd is the file descriptor for communication(connfd)
	//pkt is a pointer to the HEADER of the PACKET that we want to store data to
	//datap is the pointer to the variable we want to store payload(could be null)

	//first, we read in the packet
	int bytes_read = 0, bytes_to_read = 0;
	bytes_to_read = sizeof(XACTO_PACKET); //NUMBER OF BYTES TO READ FROM FD
	while(bytes_to_read > 0){//while there are bytes to read
		if((bytes_read = read(fd, pkt, bytes_to_read)) <= 0) { //PERFORM READ
			//BAD READ
			return -1;
		}
		//read operated fine
		bytes_to_read -= bytes_read; //decrement this loop
	}
	//Header has been read in, flip the order
	pkt->size = ntohl(pkt->size);
	pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
	pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);
	//Order flipped
	//Check for data
	if(pkt->size > 0){ //data was not null, we need to invoke another read
		//malloc datap
		*datap = calloc(pkt->size + 1, sizeof(char));
		bytes_to_read = pkt->size;//Get the payload size
		while(bytes_to_read > 0){//while there are bytes to read
			if((bytes_read = read(fd, *datap, bytes_to_read)) <= 0) { //PERFORM READ
				//BAD READ
				free(datap);
				return -1;
			}
			//read operated fine
			bytes_to_read -= bytes_read; //decrement this loop
		}
		//payload has been read in
	}
	return 0;
}



