
#include "stdafx.h"
#include "businessUtils.h"

extern SOCKET listenSock;

void checksum() {
	//return true;
};


void cleanUpClient(Client *client) {
	closesocket(client->sock);
	if (client->file != NULL)
		fclose(client->file);
	remove(client->fileName);
	client->operation = -1;
	client->isTransfering = false;
	client->key = 0;
	client->sock = 0;
}

/*
Send error message to socket specified
*/
void procErr(Client *client) {
	cleanUpClient(client);
	if (sendPacket(client->sock, OP_ERR, 0, ""))
		printf("Cannot send error message to client.\n");
}


/*
Receive file from specified sock, encode/decode
and send processed file back.
*/
void procEncodeDecode(Client *client) {
	// Declare variables
	short iRetVal;
	char buff[FILEBUFF_SIZE];
	char fileNameNew[FILENAME_LEN + 5];
	long int bytesRead;

	printf("Processing file \'%s\'...\n", client->fileName);
	// Open file and create new temp file
	snprintf(fileNameNew, FILENAME_LEN + 5, "%s.new", client->fileName);
	FILE* fnew = fopen(fileNameNew, "wb");
	if (fnew == NULL) {
		procErr(client);
		return;
	}

	// Process received file and save in new file
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

	// Send processed file to specified sock and remove temp files
	iRetVal = sendFile(client->sock, fileNameNew);
	remove(client->fileName);
	remove(fileNameNew);
	if (iRetVal == SOCKET_ERROR) {
		procErr(client);
	}
	printf("Completed request from client at socket %d. Temp files removed.\n", client->sock);
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
	FILE* f;
	client->key = atoi(payload);
	client->operation = opCode;
	client->isTransfering = true;
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
	if (len == 0 && client->isTransfering) {
		client->isTransfering = false;
		printf("Completed receiving file %s from client at socket %d.\n", client->fileName, client->sock);
		
		checksum();

		procEncodeDecode(client);
		return;
	}
	else {
		fwrite(payload, 1, len, client->file);
		printf("Received from client at socket %d: Partition %d\n", client->sock, ++client->filePart);
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
	char		payload[BUFF_SIZE];
	SOCKET		connSock;
	Client		clients[WSA_MAXIMUM_WAIT_EVENTS];
	DWORD		nEvents = 0;
	DWORD		index;
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
				break;
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
			//reset event
			WSAResetEvent(events[index]);
		}

		if (sockEvent.lNetworkEvents & FD_READ) {
			//Receive message from client
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				printf("Error %d! FD_READ failed.\n", sockEvent.iErrorCode[FD_READ_BIT]);
				break;
			}

			iRetVal = recvPacket(clients[index].sock, &opCode, &len, payload, BUFF_SIZE - 1);
			if (iRetVal == 0) {
				switch (opCode) {
				case OP_FILE:
					if (clients[index].isTransfering == false) {
						procErr(&clients[index]);
						break;
					}
					procReceiveFilePart(&clients[index], len, payload);
					break;
				case OP_DEC:
				case OP_ENC:
					if (clients[index].isTransfering) {
						procErr(&clients[index]);
						break;
					}
					procInitializeRequest(&clients[index], opCode, payload);
					break;
				default:
					procErr(&clients[index]);
				}
				//iRetVal = sendRes(clients[index], payload, strlen(buff));
				WSAResetEvent(events[index]);
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
				break;
			}
			printf("Client at socket %d closed connection.\n", clients[index].sock);
			//Release socket and event
			cleanUpClient(&clients[index]);
			WSACloseEvent(events[index]);
			nEvents--;
		}
	}
}