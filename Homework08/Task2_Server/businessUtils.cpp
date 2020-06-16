
#include "stdafx.h"
#include "businessUtils.h"

extern SOCKET listenSock;
MD5 md5;

void cleanUpClient(Client *client) {
	closesocket(client->sock);
	if (client->file != NULL)
		fclose(client->file);
	remove(client->fileName);
	client->operation = -1;
	client->isTransfering = false;
	client->isCompleted = true;
	client->filePart = 0;
	client->key = 0;
	client->sock = 0;
}

/*
Send error message to socket specified
*/
void procErr(Client *client) {
	int iRetVal;
	printf("Client %d informed corrupted file.", client->sock);
	// Send processed file to specified sock and remove temp files
	iRetVal = sendFile(client->sock, client->fileNameNew, client->fileNewDigest);
	client->isCompleted = true;
	if (iRetVal == SOCKET_ERROR) {
		printf("Fail to send to client at socket %d. Temp files removed.\n", client->sock);
		remove(client->fileName);
		remove(client->fileNameNew);
		cleanUpClient(client);
		return;
	}
	printf("Completed request from client at socket %d.\n", client->sock);
}

/*
Receive file from specified sock, encode/decode
and send processed file back.
*/
void procEncodeDecode(Client *client) {
	// Declare variables
	char buff[FILEBUFF_SIZE];
	long int bytesRead;

	printf("Processing file \'%s\'...\n", client->fileName);
	// Open file and create new temp file
	snprintf(client->fileNameNew, FILENAME_LEN + 5, "%s.new", client->fileName);
	FILE* fnew = fopen(client->fileNameNew, "wb");
	client->file = fopen(client->fileName, "rb");
	if (fnew == NULL || client->file == NULL) {
		return;
	}

	// Process received file and save in new file
	fseek(client->file, 0, SEEK_SET);
	switch (client->operation) {
	case OP_ENC:
		while (bytesRead = fread(buff, 1, FILEBUFF_SIZE, client->file)) {
			for (int i = 0; i < bytesRead; ++i)
				buff[i] = (char)((short)buff[i] + client->key) % 256;
			fwrite(buff, 1, bytesRead, fnew);
		}
		printf("Encoding file \'%s\' completed. Sending to client.\n", client->fileName);
		break;
	case OP_DEC:
		while (bytesRead = fread(buff, 1, FILEBUFF_SIZE, client->file)) {
			for (int i = 0; i < bytesRead; ++i)
				buff[i] = (char)((short)buff[i] - client->key) % 256;
			fwrite(buff, 1, bytesRead, fnew);
		}
		printf("Decoding file \'%s\' completed. Sending to client.\n", client->fileName);
		break;
	}
	fclose(client->file);
	fclose(fnew);
	
	strcpy(client->fileNewDigest, md5.digestFile(client->fileNameNew));
}


/*
Generate a new random file name
*/
void genRandName(char* fileName) {
	static char* characters = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefjhijklmnopqrstuvwxyz";
	srand((unsigned int)time(NULL));
	// Randomly choose a character
	for (int i = 0; i < FILENAME_LEN; i++) {
		fileName[i] = characters[rand() % 62];
	}
	fileName[FILENAME_LEN] = 0;
}

void procInitializeRequest(Client *client, short opCode, char* payload) {
	printf("Received %s request from client %d.\n", opCode == OP_ENC ? "encode" : "decode", client->sock);
	FILE* f;
	client->key = atoi(payload);
	client->operation = opCode;
	client->isTransfering = true;
	client->isCompleted = false;
	client->filePart = 0;

	// Generate random unique file name
	genRandName(client->fileName);
	while (f = fopen(client->fileName, "rb")) {
		fclose(f);
		genRandName(client->fileName);
	};

	// Keep file open for appending and reading
	client->file = fopen(client->fileName, "a+b");
}

void procReceiveFilePart(Client *client, short len, char* payload) {
	int iRetVal;

	if (client->isTransfering == true) {
		// If client is in transferring state and server receive a message len 0, client has completed sending file.
		if (len == 0) {
			client->isTransfering = false;
			fclose(client->file);
			printf("Completed receiving file %s from client at socket %d.\n", client->fileName, client->sock);
		}
		else {
			fwrite(payload, 1, len, client->file);
			printf("Received from client at socket %d: Partition %d\n", client->sock, ++client->filePart);
		}
	}
	// If client is in non-transferring state but request is not completed,
	// meaning client is sending MD5 code
	else if (client->isCompleted == false) {
		// If file integrity is confirmed, send OP_FILE message len 0 to client
		// run decode/encode, and send result to client
		if (strcmp(md5.digestFile(client->fileName), payload) == 0) {
			printf("MD5 check returns success code for file from %d.\n", client->sock);
			iRetVal = sendPacket(client->sock, OP_FILE, 0, "");

			procEncodeDecode(client);
			
			// Send processed file to specified sock and remove temp files
			iRetVal = sendFile(client->sock, client->fileNameNew, client->fileNewDigest);
			client->isCompleted = true;
			if (iRetVal == SOCKET_ERROR) {
				printf("Fail to send to client at socket %d. Temp files removed.\n", client->sock);
				remove(client->fileName);
				remove(client->fileNameNew);
				cleanUpClient(client);
				return;
			}
			printf("Completed request from client at socket %d.\n", client->sock);
		}
		// If file is corrupted, send error message to client
		else {
			printf("[!] MD5 check returns fail code for file from %d.\n", client->sock);
			client->isTransfering = true;

			// remove corrupted file
			if (client->file != NULL)
				fclose(client->file);
			remove(client->fileName);
			client->filePart = 0;

			// Open file again for appending and reading
			client->file = fopen(client->fileName, "a+b");

			if (sendPacket(client->sock, OP_ERR, 0, "")) {
				printf("Cannot send error message to client.\n");
				cleanUpClient(client);
			}
		}
	}
	// If client is in non-transferring state and request is completed,
	// meaning client is sending to confirm file integrity
	else {
		remove(client->fileName);
		remove(client->fileNameNew);
		printf("Client at socket %d confirmed file integrity. Temp files on server removed.\n", client->sock);
	}
};

/*
Application driver. Initialize variables and run loop to accept new connection,
process request and send responses to socket.
*/
void run() {
	// Initialize Variables
	int			iRetVal;
	short		opCode;
	short		len;
	char		payload[FILEBUFF_SIZE];
	SOCKET		connSock;
	Client		clients[WSA_MAXIMUM_WAIT_EVENTS];
	DWORD		nEvents = 0;
	DWORD		index = 0;
	WSAEVENT	events[WSA_MAXIMUM_WAIT_EVENTS];
	WSANETWORKEVENTS sockEvent;

	clients[0].sock = listenSock;
	events[0] = WSACreateEvent();
	nEvents++;

	WSAEventSelect(clients[0].sock, events[0], FD_ACCEPT | FD_CLOSE);

	while (1) {
		//wait for network events on all socket
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) {
			printf("Error %d! WSAWaitForMultipleEvents() failed.\n", WSAGetLastError());
			break;
		}

		index -= WSA_WAIT_EVENT_0;
		WSAEnumNetworkEvents(clients[index].sock, events[index], &sockEvent);

		if (sockEvent.lNetworkEvents & FD_ACCEPT) {
			if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
				printf("Error %d! FD_ACCEPT failed.\n", sockEvent.iErrorCode[FD_READ_BIT]);
				continue;
			}

			if (acceptConn(&connSock) == 1) {
				printf("[!] Accept failed.\n");
				continue;
			}

			//Add new socket into socks array
			if (nEvents == WSA_MAXIMUM_WAIT_EVENTS) {
				printf("[!] Too many clients.\n");
				closesocket(connSock);
			}
			else
				for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++)
					if (clients[i].sock == 0) {
						clients[i].sock = connSock;
						events[i] = WSACreateEvent();
						WSAEventSelect(clients[i].sock, events[i], FD_READ | FD_CLOSE);
						nEvents++;
						break;
					}
		}

		if (sockEvent.lNetworkEvents & FD_READ) {
			//Receive message from client
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				printf("Error %d! FD_READ failed.\n", sockEvent.iErrorCode[FD_READ_BIT]);
				continue;
			}

			iRetVal = recvPacket(clients[index].sock, &opCode, &len, payload, FILEBUFF_SIZE);
			if (iRetVal == 0) {
				switch (opCode) {
				case OP_FILE:
					// If client is not in file transferring state then discard message and clean up client.
					if (clients[index].isTransfering == false && clients[index].isCompleted) {
						cleanUpClient(&clients[index]);
						break;
					}
					procReceiveFilePart(&clients[index], len, payload);
					break;
				case OP_DEC:
				case OP_ENC:
					// If client is in file transferring state then discard message and clean up client.
					if (clients[index].isTransfering) {
						cleanUpClient(&clients[index]);
						break;
					}
					procInitializeRequest(&clients[index], opCode, payload);
					break;
				case OP_ERR:
					procErr(&clients[index]);
					break;
				default:
					cleanUpClient(&clients[index]);
				}
			} else {
				// Cleanup socket
				cleanUpClient(&clients[index]);
				WSACloseEvent(events[index]);
				nEvents--;
			}
		}

		if (sockEvent.lNetworkEvents & FD_CLOSE) {
			if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
				printf("Error %d! FD_READ failed.\n", sockEvent.iErrorCode[FD_CLOSE_BIT]);
				continue;
			}
			printf("Client at socket %d closed connection.\n", clients[index].sock);
			//Release socket and event
			cleanUpClient(&clients[index]);
			WSACloseEvent(events[index]);
			nEvents--;
		}
	}
}