/*
Client first initiate a new connection with Server. Client then
sends a file name and a MD5 hash to Server. Client will send multiple
file content messages to server. Once the file has been fully
transferred, Client sends a file content message with length 0 to Server.

If any of the messages that server receives from client does
not comply to the message format, server sends back a bad request
error message.
If after checking the md5, server sees that the file is corrupted,
server sends back a file corrupted error error message.
If the file is successfully transferred but server fails to save
it for some reason, server sends back an internal server error
message.

Message format:
[opCode] [length] [offset] [payload]
 1 byte   4 byte   4 byte

*/

#ifndef BUSINESSUTILS_H_
#define BUSINESSUTILS_H_

#include <stdlib.h>
#include "dataStructures.h"

#define OP_FILENAME			0
#define OP_FILECONTENT		1
#define OP_FILEDIGEST		2
#define OP_ERROR			3
#define	OP_OK				4

#define ERROR_SERVER_FAIL	"500"
#define	ERROR_BAD_REQUEST	"400"
#define	ERROR_FILE_CORRUPTED "450"

MD5 md5;

/*
Function construct a message and prepare perIoData for sending.
[IN/OUT] perQueueItem:		a PER_QUEUE_ITEM pointer containing
							the socket information.
[IN] opCode:		the opCode to be inject in the message
[IN] payloadLength: the length of the payload to be inject in the message
[IN] offset:		the file part offset value to be inject in the message
[IN] payload:		the payload to be inject in the message
*/
void packMessage(LPPER_QUEUE_ITEM perQueueItem, int opCode, int payloadLength, int offset, char* payload) {
	ZeroMemory(&(perQueueItem->perIoData->overlapped), sizeof(OVERLAPPED));
	perQueueItem->perIoData->dataBuff.len = sizeof(MESSAGE);
	perQueueItem->perIoData->recvBytes = 0;
	perQueueItem->perIoData->sentBytes = 0;
	perQueueItem->perIoData->dataBuff.buf = perQueueItem->perIoData->buffer;
	perQueueItem->perIoData->operation = SEND;
	
	LPMESSAGE message = (LPMESSAGE)perQueueItem->perIoData->buffer;
	message->opCode = opCode;
	message->payloadLength = payloadLength;
	message->offset = offset;
	memcpy(message->payload, payload, payloadLength);
}

/*
Function allocates memory for fileProp, copy file name from buffer
into fileProp.
[IN/OUT] queueItem:		the PER_QUEUE_ITEM struct containing
						operation information.
*/
int processOpFileName(LPPER_QUEUE_ITEM queueItem) {

	printf("Received file upload request from client %d.\n", queueItem->socket);

	LPMESSAGE message = (LPMESSAGE)queueItem->buff;
	// If fileProp is not null, server has already received a file name from client
	// so send bad request error message.
	if (queueItem->perIoData->fileProp != NULL) {
		printf("bad request posed at opfile name\n");
		packMessage(queueItem, OP_ERROR, 4, 0, ERROR_BAD_REQUEST);
		return 1;
	}

	queueItem->perIoData->fileProp = (LPFILE_TRANSFER_PROPERTY)GlobalAlloc(GPTR, sizeof(FILE_TRANSFER_PROPERTY));
	LPFILE_TRANSFER_PROPERTY fileProp = queueItem->perIoData->fileProp;

	memcpy(fileProp->fileName, message->payload, message->payloadLength);

	// Keep file open for writing
	fileProp->file = fopen(fileProp->fileName, "wb");
	if (fileProp->file == NULL) {
		packMessage(queueItem, OP_ERROR, 4, 0, ERROR_SERVER_FAIL);
	}
	packMessage(queueItem, OP_OK, 0, 0, "");

	return 1;
}

/*
Function copies file digest from buffer into fileProp and change
operation state to transfering.
[IN/OUT] queueItem:		the PER_QUEUE_ITEM struct containing
						operation information.
*/
int processOpFileDigest(LPPER_QUEUE_ITEM queueItem) {
	printf("Received MD5 from client %d.\n", queueItem->socket);

	LPMESSAGE message = (LPMESSAGE)queueItem->buff;
	LPFILE_TRANSFER_PROPERTY fileProp = queueItem->perIoData->fileProp;

	// If fileProp is null, the process hasn't been initiated.
	// If isTransfering is true, file is already being transferred.
	// Either way, send bad request error message.
	if (fileProp == NULL || fileProp->isTransfering == true) {
		printf("bad request posed at opfile digest\n");
		packMessage(queueItem, OP_ERROR, 4, 0, ERROR_BAD_REQUEST);
		return 1;
	}

	fileProp->isTransfering = true;
	memcpy(fileProp->digest, message->payload, message->payloadLength);

	packMessage(queueItem, OP_OK, 0, 0, "");
	return 1;
}

/*
Function write file content from buffer into a file specified in
fileProp.
[IN/OUT] queueItem:		the PER_QUEUE_ITEM struct containing
						operation information.
*/
int processOpFileContent(LPPER_QUEUE_ITEM queueItem) {
	LPMESSAGE message = (LPMESSAGE)queueItem->buff;
	LPFILE_TRANSFER_PROPERTY fileProp = queueItem->perIoData->fileProp;

	// If fileProp is null, or is not transferring, the process hasn't been initiated.
	// So, send bad request error message.
	if (fileProp == NULL || fileProp->isTransfering == false) {
		packMessage(queueItem, OP_ERROR, 4, 0, ERROR_BAD_REQUEST);
		printf("bad request posed at opfile content\n");
		return 1;
	}

	// If client is in transferring state and server receive a message len 0, client has completed sending file.
	if (message->payloadLength > 0) {
		fseek(fileProp->file, message->offset, SEEK_SET);
		int writenBytes = fwrite(message->payload, 1, message->payloadLength, fileProp->file);
		if (writenBytes < message->payloadLength) {
			packMessage(queueItem, OP_ERROR, 4, 0, ERROR_SERVER_FAIL);
		}
		printf("Received from client at socket %d: Partition %d\n", queueItem->socket, ++fileProp->filePart);
		return 0;
	}
	else {
		fileProp->isTransfering = false;
		fclose(fileProp->file);
		printf("Completed receiving file %s from client at socket %d.\n", fileProp->fileName, queueItem->socket);

		// Check file integrity
		if (strcmp(md5.digestFile(fileProp->fileName), fileProp->digest) == 0) {
			printf("MD5 check returns success code for file from %d.\n", queueItem->socket);
			packMessage(queueItem, OP_OK, strlen(fileProp->fileName), 0, fileProp->fileName);
			printf("Completed request from client at socket %d.\n", queueItem->socket);
			GlobalFree(fileProp);
		}
		// If file is corrupted, send error message to client
		else {
			printf("[!] MD5 check returns fail code for file from %d.\n", queueItem->socket);
			packMessage(queueItem, OP_ERROR, 4, 0, ERROR_FILE_CORRUPTED);
			fileProp->isTransfering = true;

			// remove corrupted file
			if (fileProp->file != NULL)
				fclose(fileProp->file);
			remove(fileProp->fileName);
			fileProp->filePart = 0;

			// Open file again for writing
			fileProp->file = fopen(fileProp->fileName, "wb");
			if (fileProp->file == NULL) {
				packMessage(queueItem, OP_ERROR, 4, 0, ERROR_SERVER_FAIL);
			}
		}
	}
	return 1;
};

/*
Extract information from request and call corresponding process function
Function returns 1 if a response is ready to be sent, else returns 0
[IN] sock:		the socket that's sending the request
[IN/OUT] buff:	a char array which contains the request, and stores response
				after the request has been processed
[OUT] len:		the length of the payload of the output
*/
int parseAndProcess(LPPER_QUEUE_ITEM queueItem) {
	LPMESSAGE message = (LPMESSAGE)queueItem->buff;

	// Process request
	switch (message->opCode) {
	case OP_FILENAME:
		return processOpFileName(queueItem);
	case OP_FILEDIGEST:
		return processOpFileDigest(queueItem);
	case OP_FILECONTENT:
		return processOpFileContent(queueItem);
	default:
		printf("bad request at default\n");
		packMessage(queueItem, OP_ERROR, 4, 0, ERROR_BAD_REQUEST);
		return 1;
	}
}
#endif