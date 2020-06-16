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

int send_s(SOCKET s, char* buff, int len);
int recv_s(SOCKET s, char* buff);
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
	char buff[BUFF_SIZE];

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
		return 1;
	}
	printf("Client started and connected to server.\n");

	while (1) {
		// Get input
		printf("Send to server:\n\t");
		gets_s(buff, BUFF_SIZE);
		if (strlen(buff) == 0 || buff[0] == '\r' || buff[0] == '\n')
			break;

		// Send message
		iRetVal = send_s(client, buff, strlen(buff));
	
		if (iRetVal == SOCKET_ERROR) {
			printf("Error %d! Cannot send message!", WSAGetLastError());
			break;
		}

		// Receive message
		iRetVal = recv_s(client, buff);
		if (iRetVal == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT)
				printf("Time-out! No response from server.");
			else
				printf("Error %d! Cannot receive message!", WSAGetLastError());
			break;
		}
		else {
			processMsg(buff);
		}
		printf("\n");
	}

	// Close socket & cleanup
	closesocket(client);
	WSACleanup();
	return 0;
}

// Add length header to buffer and send to socket s
// return 0 if sent successfully
// return SOCKET_ERROR if fail to send
int send_s(SOCKET s, char* buff, int len) {
	int iRetVal, lenLeft = len + 4, idx = 0;
	char tempBuff[BUFF_SIZE + 4];
	
	// Put length in the first 4 bytes of tempBuff
	// Copy buff into tempBuff
	int tempLenLeft = lenLeft;
	for (int i = 3; i >= 0; i--) {
		tempBuff[i] = tempLenLeft & 0b11111111;
		tempLenLeft >>= 8;
	}
	strcpy_s(tempBuff + 4, BUFF_SIZE, buff);
	
	// Send message
	while (lenLeft > 0) {
		iRetVal = send(s, tempBuff + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR)
			return SOCKET_ERROR;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	return 0;
}

// Receive message from socket s.
// Extract and remove length header from buffer
// return 0 if received successfully
// return SOCKET_ERROR if fail to receive
int recv_s(SOCKET s, char* buff) {
	int iRetVal, lenLeft = 0, idx = 0;

	// Receive 1 message to extract length
	iRetVal = recv(s, buff, BUFF_SIZE, 0);
	if (iRetVal == SOCKET_ERROR)
		return SOCKET_ERROR;

	// Extract length and remove the length header from buff
	for (int i = 0; i < 4; i++) {
		lenLeft = (lenLeft << 8) | buff[i];
	}

	// Reduce lenLeft and increment index
	lenLeft -= iRetVal;
	idx += iRetVal;

	// Receive message
	while (lenLeft > 0) {
		iRetVal = recv(s, buff, BUFF_SIZE, 0);
		if (iRetVal == SOCKET_ERROR)
			return SOCKET_ERROR;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	buff[idx] = 0;
	strcpy_s(buff, BUFF_SIZE, buff + 4);

	return 0;
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