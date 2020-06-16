#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "md5.h"
#pragma comment (lib, "Ws2_32.lib")

#define FILENAME_SIZE	150
#define DIGEST_SIZE		33
#define BUFSIZE			8192
#define PAYLOAD_SIZE	8150

#define FILEPATH_SIZE	1024
#define KEY_SIZE 4

WSAData wsaData;
sockaddr_in serverAddr;

typedef struct {
	short opCode;
	int payloadLength;
	int offset;
	char payload[PAYLOAD_SIZE];
} MESSAGE, *LPMESSAGE;

/*
Initialize network environment, return 0 if initialize successful else return 1
[IN] serverIpAddress:	the server IP Address to initate a connection with.
[IN] serverPortNumber:	the server IP ports to initate a connection with.
*/
int initConnection(char* serverIpAddress, short serverPortNumber) {
	int iRetVal;
	unsigned long ulRetVal;

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d", iRetVal);
		return 1;
	}

	// Specifying server address
	ulRetVal = inet_addr(serverIpAddress);
	if (ulRetVal == INADDR_NONE) {
		printf("Cannot identify server IP address. Exiting...");
		WSACleanup();
		return 1;
	}
	if (serverPortNumber == 0) {
		printf("Cannot identify server port number.");
		WSACleanup();
		return 1;
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ulRetVal;
	serverAddr.sin_port = htons(serverPortNumber);

	return 0;
}

/*
Initiate a new connection with server, return 0 if initialize successful else return 1
[OUT] outSock:	a SOCKET variable to store the new socket created by the function
*/
int connectToServer(SOCKET* outSock) {
	*outSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int iRetVal = connect(*outSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (iRetVal == SOCKET_ERROR) {
		printf("Error %d! Cannot connect to server.", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	return 0;
}

/*
Construct packet and send to the specified socket. Returns 0 if sends successfully
else return 1
[IN] socket:	the socket to which the message will be sent
[IN] opCode:	the opCode to be encoded in the message
[IN] payloadLength:	the length of the payload
[IN] offset:	the offset value in a file (used only when transferring file parts)
[IN] payload:	char array containing the payload
*/
int sendMessage(SOCKET socket, int opCode, int payloadLength, int offset, char* payload) {
	int iRetVal, lenLeft = sizeof(MESSAGE), idx = 0;
	char buff[sizeof(MESSAGE)];
	LPMESSAGE message = (LPMESSAGE)buff;

	// Construct message
	message->opCode = opCode;
	message->payloadLength = payloadLength;
	message->offset = offset;
	memcpy(message->payload, payload, payloadLength);

	// Send message
	while (lenLeft > 0) {
		iRetVal = send(socket, buff + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR) {
			printf("[!] Error %d! Cannot send to server!\n", WSAGetLastError());
			return 1;
		}
		lenLeft -= iRetVal;
		idx += iRetVal;
		printf("Sent %d bytes.\n", iRetVal);
	}
	return 0;
}

/*
Receive a packet from the specified socket, extract opcode, payload length,
offset value and payload. Returns 0 if received successfully, else return 1
[IN] socket:	the socket from which the message will be received
[OUT] opCode:	an int to store the opCode extracted from the message
[OUT] payloadLength:	an int to store the payload length extracted from the message
[OUT] offset:	an int to store the offset value extracted from the message
[OUT] payload:	a char array to store the payload extracted from the message
*/
int recvMessage(SOCKET socket, int* opCode, int* payloadLength, int* offset, char* payload) {
	short iRetVal, lenLeft = sizeof(MESSAGE), idx = 0;
	char buff[sizeof(MESSAGE)];
	LPMESSAGE message = (LPMESSAGE)buff;

	// Receive payload
	while (lenLeft > 0) {
		iRetVal = recv(socket, buff + idx, lenLeft, 0);
		if (iRetVal <= 0) {
			printf("Error code %d while receiving.\n", WSAGetLastError());
			return iRetVal;
		}
		lenLeft -= iRetVal;
		idx += iRetVal;
	}

	*opCode = message->opCode;
	*payloadLength = message->payloadLength;
	*offset = message->offset;
	memcpy(payload, message->payload, *payloadLength);

	return 0;
}

#endif