
#include "stdafx.h"
#include "ioUtils.h"

#pragma comment (lib, "Ws2_32.lib")

// Shared variables
WSAData wsaData;
SOCKET client;
sockaddr_in serverAddr;

int initConnection(char* serverIpAddr, short serverPortNumbr) {
	int iRetVal;
	unsigned long ulRetVal;

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
		printf("Cannot identify server port number.");
		WSACleanup();
		return 1;
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ulRetVal;
	serverAddr.sin_port = htons(serverPortNumbr);

	// Construct socket and set timeout
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	// Request to connect to server
	iRetVal = connect(client, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (iRetVal == SOCKET_ERROR) {
		printf("Error %d! Cannot connect to server.", WSAGetLastError());
		ioCleanup();
		return 1;
	}

	return 0;
}

void ioCleanup() {
	closesocket(client);
	WSACleanup();
}

/*
Encode length header, send header & msg to socket s
return 0 if sent successfully
return SOCKET_ERROR if fail to send
*/
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

/*
Receive message from socket s and fill into buffer
return RECVMSG_ERROR if message is larger than bufflen
return 1 if received successfully
return 0 if socket closed connection
return SOCKET_ERROR if fail to receive
*/
int recv_s(SOCKET s, char* buff, int buffsize) {
	int iRetVal, lenLeft = 0, idx = 0;

	// Receive 4 bytes and decode length
	iRetVal = recv(s, buff, 4, 0);
	if (iRetVal <= 0)
		return iRetVal;
	for (int i = 0; i < 4; i++) {
		lenLeft = ((lenLeft << 8) + 0xff) & buff[i];
	}

	// If message length is larger than buffsize
	// msg is corrupted => discard message.
	if (lenLeft > buffsize) {
		while (lenLeft > 0) {
			iRetVal = recv(s, buff,
				(lenLeft > buffsize) ? buffsize : lenLeft, 0);
			if (iRetVal <= 0)
				return iRetVal;
			lenLeft -= iRetVal;
		}
		return RECVMSG_CORRUPT;
	}

	// Receive message
	while (lenLeft > 0) {
		iRetVal = recv(s, buff + idx, lenLeft, 0);
		if (iRetVal <= 0)
			return iRetVal;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	buff[idx] = 0;
	return 1;
}

int sendAndRecv(char* buff, int buffLen) {
	int iRetVal;
	
	// Send message
	iRetVal = send_s(client, buff, strlen(buff));
	if (iRetVal == SOCKET_ERROR) {
		printf("[!] Error %d! Cannot send to server!\n", WSAGetLastError());
		return 1;
	} 
	printf("[+] Waiting for server.\n");

	// Receive response
	iRetVal = recv_s(client, buff, buffLen);
	switch (iRetVal) {
	case SOCKET_ERROR:
		if (WSAGetLastError() == WSAETIMEDOUT)
			printf("[!] Time-out! No response from server.\n");
		else
			printf("[!] Error %d! Cannot receive message from server!\n", WSAGetLastError());
		return 1;
	case 0:
		printf("[!] Server closed connection.\n");
		return 1;
	case RECVMSG_CORRUPT:
		printf("[!] Response from server corrupted.\n");
		return 1;
	default:
		return 0;
	}
}
