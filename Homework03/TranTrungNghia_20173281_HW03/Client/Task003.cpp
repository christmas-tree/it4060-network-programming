#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#define BUFF_SIZE 2048
#define SERVER_PORT_DEFAULT 5500
#define SERVER_FAILURE -1
#define SERVER_SUCCESS 1

unsigned int getInput(char** pbuff, unsigned int curlen);
int send_s(SOCKET s, char* buff, int len);
int recv_s(SOCKET s, char** pbuff, int curBuffLen);
void processMsg(char* msg);

int main(int argc, char* argv[])
{
	// Validate parameters and initialize variables
	if (argc != 3) {
		printf("Wrong arguments! Please enter in format: client [ServerIpAddress] [ServerPortNumber]");
		return 1;
	}

	char* serverIpAddr = argv[1];
	short serverPortNumbr = atoi(argv[2]);

	WSAData wsaData;
	int iRetVal;
	unsigned long ulRetVal;
	char* buff = (char*)malloc(BUFF_SIZE);
	unsigned int curBuffLen = BUFF_SIZE;

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d", iRetVal);
		return 1;
	}

	// Specifying server address
	ulRetVal = inet_addr(serverIpAddr);
	if (ulRetVal == INADDR_NONE) {
		printf("Cannot identify server IP address. Exiting...");
		WSACleanup();
		return 1;
	}
	if (serverPortNumbr == 0) {
		printf("Cannot identify server port number. Using default value: %d", SERVER_PORT_DEFAULT);
		serverPortNumbr = SERVER_PORT_DEFAULT;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ulRetVal;
	serverAddr.sin_port = htons(serverPortNumbr);

	// Construct socket and set timeout
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	// Request to connect to server
	iRetVal = connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (iRetVal == SOCKET_ERROR) {
		printf("Error %d! Cannot connect to server.", WSAGetLastError());
		free(buff);
		closesocket(client);
		WSACleanup();
		return 1;
	}
	printf("Client started and connected to server.\n");

	while (1) {
		// Get input
		printf("Send to server:\n\t");
		curBuffLen = getInput(&buff, curBuffLen);
		if (strlen(buff) == 0)
			break;

		// Send message
		iRetVal = send_s(client, buff, strlen(buff));
	
		if (iRetVal == SOCKET_ERROR) {
			printf("Error %d! Cannot send message!", WSAGetLastError());
			break;
		}

		// Receive message
		iRetVal = recv_s(client, &buff, curBuffLen);
		if (iRetVal == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT)
				printf("Time-out! No response from server.");
			else
				printf("Error %d! Cannot receive message!", WSAGetLastError());
			break;
		}
		else if (iRetVal == 0) {
			printf("Server closed connection.");
		}
		else {
			processMsg(buff);
		}
		printf("\n");
	}

	// Close socket & cleanup
	free(buff);
	closesocket(client);
	WSACleanup();
	return 0;
}

// Get input from stdin into *ppOutstr
// reallocate if input is longer than
// allocated size and return new size
unsigned int getInput(char** pbuff, unsigned int curlen) {
	char c;
	unsigned int i = 0;
	while ((c = getchar()) != '\n' && c != EOF) {
		(*pbuff)[i++] = (char)c;
		if (i == curlen) {
			curlen = i + BUFF_SIZE;
			*pbuff = (char*) realloc(*pbuff, curlen);
		}
	}
	(*pbuff)[i] = 0;
	return curlen;
}

// Encode length header, send header & msg to socket s
// return 0 if sent successfully
// return SOCKET_ERROR if fail to send
int send_s(SOCKET s, char* buff, int len) {
	int iRetVal, lenLeft = len, idx = 0;
	char tempLenBuff[4];

	// Send length
	for (int i = 3; i >= 0; i--) {
		tempLenBuff[i] = len & 0b11111111;
		len >>= 8;
	}
	iRetVal = send(s, tempLenBuff, 4, 0);
	if (iRetVal == SOCKET_ERROR)
		return SOCKET_ERROR;

	// Send message
	while (lenLeft > 0) {
		iRetVal = send(s, buff + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR)
			return SOCKET_ERROR;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	return 0;
}

// Receive message from socket s and fill into buffer
// realloc buffer if there is not enough space
// return buffer size if received successfully
// return 0 if socket closed connection
// return SOCKET_ERROR if fail to receive
int recv_s(SOCKET s, char** pbuff, int curBuffLen) {
	int iRetVal, lenLeft = 0, idx = 0;

	// Receive 4 bytes and decode length
	iRetVal = recv(s, *pbuff, 4, 0);
	if (iRetVal <= 0)
		return iRetVal;
	for (int i = 0; i < 4; i++) {
		lenLeft = ((lenLeft << 8) + 0xff) & (*pbuff)[i];
	}

	// Realloc buffer if there is not enough space
	if (curBuffLen <= lenLeft) {
		curBuffLen += BUFF_SIZE;
		*pbuff = (char*)realloc(*pbuff, curBuffLen);
	}

	// Receive message
	while (lenLeft > 0) {
		iRetVal = recv(s, *pbuff + idx, lenLeft, 0);
		if (iRetVal <= 0)
			return iRetVal;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	(*pbuff)[idx] = 0;
	return curBuffLen;
}


// Read first byte for message status
// and print the message to screen.
void processMsg(char* msg) {
		if (msg[0] == SERVER_FAILURE) {
		printf("Error: String contains number.");
	}
	else if (msg[0] == SERVER_SUCCESS) {
		printf("Number of words: %s", msg + 1); // Print string except the first byte.
	}
	else {
		printf("Message from server corrupted!");
	}
	printf("\n");
}