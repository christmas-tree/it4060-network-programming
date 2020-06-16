
#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#define BUFF_SIZE 2048
#define SERVER_PORT_DEFAULT 5500
#define DNS_RESOLVE_FAILURE -1
#define DNS_RESOLVE_SUCCESS 1

void processMsg(char* message) {
	// Process the message and print to screen.

	if (message[0] == DNS_RESOLVE_FAILURE) {
		printf("Not found information");
	}
	else if (message[0] == DNS_RESOLVE_SUCCESS) {
		printf("%s", message + 1); // Print string except the first byte.
	}
	else {
		printf("Message from server corrupted!");
	}
	printf("\n");
}

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
	SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	printf("Client started.\n");

	// Communicate with Server
	int serverAddrLen = sizeof(serverAddr);
	while (1) {
		// Get input
		printf("[Q]: ");
		gets_s(buff, BUFF_SIZE);
		if (strlen(buff) == 0 || buff[0] == '\r' || buff[0] == '\n')
			break;

		// Send message to Server
		iRetVal = sendto(client, buff, strlen(buff), 0, (sockaddr*)&serverAddr, serverAddrLen);
		if (iRetVal == SOCKET_ERROR) {
			printf("Error %d! Cannot send message!", WSAGetLastError());
			break;
		}

		// Receive result from Server
		iRetVal = recvfrom(client, buff, BUFF_SIZE-1, 0, (sockaddr*)&serverAddr, &serverAddrLen);
		if (iRetVal == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT)
				printf("Timeout! No response from server!");
			else
				printf("Error %d! No response from server!", WSAGetLastError());
			break;
		}
		else if (strlen(buff) > 0) {
			buff[iRetVal] = 0;
			processMsg(buff);
		}
		printf("\n");
	}

	// Close socket
	closesocket(client);
	WSACleanup();

    return 0;
}

