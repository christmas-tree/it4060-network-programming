
#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT_DEFAULT 5500
#define BUFF_SIZE 2048
#define SERVER_FAILURE -1
#define SERVER_SUCCESS 1

int send_s(SOCKET s, char* buff, int len);
int recv_s(SOCKET s, char** pbuff, int curBuffLen);
void processMsg(char* msg);
int countWord(char* msg);

int main(int argc, char* argv[]) {

	// Validate parameters and initialize variables
	if (argc != 2) {
		printf("Wrong argument! Please enter in format: server [ServerPortNumber]");
		return 1;
	}

	WSAData wsaData;
	int iRetVal;
	char* buff = (char*)malloc(BUFF_SIZE);
	unsigned int curBuffSize = BUFF_SIZE;

	short serverPortNumbr = (short)atoi(argv[1]);
	if (serverPortNumbr == 0) {
		printf("Fail to read server port number. Using default value: %d\n", SERVER_PORT_DEFAULT);
		serverPortNumbr = SERVER_PORT_DEFAULT;
	}

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d\n", iRetVal);
		return 1;
	}

	// Construct socket
	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Bind address to socket
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPortNumbr);
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("Error binding server address.\n");
		free(buff);
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}

	// Listen request from client
	if (listen(listenSock, 10)) {
		printf("Error %d! Cannot listen.\n", WSAGetLastError());
		free(buff);
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}
	printf("Server started!\n");

	// Communicate with client
	sockaddr_in clientAddr;
	ZeroMemory(&clientAddr, sizeof(clientAddr));
	int clientAddrLen = sizeof(clientAddr);

	while (1) {
		printf("Waiting for connect request!\n");
		SOCKET connSock;
		// accept request
		connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
		if (connSock == INVALID_SOCKET) {
			printf("Error accepting! Code: %d\n", WSAGetLastError());
		}
		else {
			printf("Server got connection from client [%s:%d].\n",
				inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

			while (1) {
				//receive message from client
				iRetVal = recv_s(connSock, &buff, curBuffSize);
				if (iRetVal == SOCKET_ERROR) {
					printf("Error code %d.\n", WSAGetLastError());
					break;
				}
				else if (iRetVal == 0) {
					printf("Client [%s:%d] closed connection.\n",
						inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
					break;
				}
				curBuffSize = iRetVal;
				printf("Receive from client [%s:%d] %s\n",
					inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff);

				// Process received message
				processMsg(buff);

				//Send result to client
				iRetVal = send_s(connSock, buff, strlen(buff));
				if (iRetVal == SOCKET_ERROR) {
					printf("Error %d!\n", WSAGetLastError());
					break;
				}
				printf("Response sent to client.\n");
			}
		}
		closesocket(connSock);
	}

	// Close socket & cleanup
	free(buff);
	closesocket(listenSock);
	WSACleanup();

	return 0;
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

// Process message in buff, output processed result
// to buff and print result to screen.
void processMsg(char* buff) {
	int wordCount = countWord(buff);
	if (wordCount == -1) {
		buff[0] = SERVER_FAILURE; // First byte indicates result status
		buff[1] = 0;
		printf("Error: String contains number.");
	}
	else {
		buff[0] = SERVER_SUCCESS;
		snprintf(buff + 1, BUFF_SIZE - 1, "%d", wordCount);
		printf("Number of words: %s", buff + 1);
	}
	printf("\n");
}

// Count the number of word in the string
// return -1 if string contains a digit
// else return word count.
int countWord(char* str) {
	int count = 0;
	bool flag = 0;
	for (int i = 0; str[i] != 0; i++) {
		if (isdigit(str[i]))
			return -1;

		//Increment count if the character being checked is not
		//a space character and the previous character is a space 
		if (str[i] == ' ' || str[i] == '\n' || str[i] == '\t')
			flag = 0;
		else if (flag == 0) {
			flag = 1;
			count++;
		}
	}
	return count;
}