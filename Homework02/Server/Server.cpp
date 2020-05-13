
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
#define MSG_SIZE 2047
#define DNS_RESOLVE_FAILURE -1
#define DNS_RESOLVE_SUCCESS 1

int isHostName(char* userInput) {
	// Function takes a char array and
	// returns 0 if it is a valid IPv4 address
	// returns 1 if it is a hostname.
	
	for (int i = 0; userInput[i] != 0; i++) {
		if (isalpha(userInput[i]))
			return 1;
	}
	return 0;
}

int getName(char* userInput, char* outResult) {
	// Function resolve hostname from char array userInput
	// containing IPv4 address and print the response to outResult,
	// returns 0 if resolve successfully, returns 1 if fail.
	
	// Declare variables
	struct in_addr addr;
	hostent* hostInfo;
	char** pAlias;

	// Setup in_addr to pass to gethostbyaddr()
	addr.s_addr = inet_addr(userInput);
	if (addr.s_addr == INADDR_NONE) {
		return 1;
	}

	// Call gethostbyaddr()
	hostInfo = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
	if (hostInfo == NULL) {
		return 1;
	}
	else {
		// Print response to char array outResult
		strcpy_s(outResult, MSG_SIZE, "Official name: ");
		strcat_s(outResult, MSG_SIZE, hostInfo->h_name);
		pAlias = hostInfo->h_aliases;
		if (*pAlias != NULL) {
			strcat_s(outResult, MSG_SIZE, "\nAllias name(s):");
			while (*pAlias != NULL) {
				strcat_s(outResult, MSG_SIZE, "\n");
				strcat_s(outResult, MSG_SIZE, *pAlias);
				pAlias++;
			}
		}
	}
	return 0;
}

int getAddress(char* userInput, char* outResult) {
	// Function resolve address from char array userInput
	// containing hostname and print the response to outResult,
	// returns 0 if resolve successfully, returns 1 if fail.
	
	// Declare variables
	DWORD dwRetval;
	struct addrinfo hints;
	struct addrinfo* result;
	struct addrinfo* pResult;
	struct sockaddr_in* sockaddr;

	// Setup the hints address info structure
	// to pass to the getaddrinfo() function
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Call getaddrinfo()
	dwRetval = getaddrinfo(userInput, "http", &hints, &result);
	if (dwRetval != 0) {
		return 1;
	}

	// Print response to char array outResult
	pResult = result;
	strcpy_s(outResult, MSG_SIZE, "Official IP: ");
	sockaddr = (struct sockaddr_in*) pResult->ai_addr;
	strcat_s(outResult, MSG_SIZE, inet_ntoa(sockaddr->sin_addr));

	pResult = pResult->ai_next;

	if (pResult != NULL) {
		strcat_s(outResult, MSG_SIZE, "\nAllias IP(s):");
		while (pResult != NULL) {
			sockaddr = (struct sockaddr_in*) pResult->ai_addr;
			strcat_s(outResult, MSG_SIZE, "\n");
			strcat_s(outResult, MSG_SIZE, inet_ntoa(sockaddr->sin_addr));
			pResult = pResult->ai_next;
		}
	}

	freeaddrinfo(result);
	return 0;
}

void processMsg(char* buff) {
	// Resolve the hostname/address contained in buff
	// and print response to buff.

	// Initialize Variables
	int retVal;
	char result[BUFF_SIZE];

	// Resolve msg and encode resolve status in
	// first byte of the response.
	if (isHostName(buff))
		retVal = getAddress(buff, result+1);
	else
		retVal = getName(buff, result+1);
	if (retVal == 1) {
		result[0] = DNS_RESOLVE_FAILURE;
		result[1] = 0;
	}
	else
		result[0] = DNS_RESOLVE_SUCCESS;

	// Print response to buff
	strcpy_s(buff, BUFF_SIZE, result);
}


int main(int argc, char* argv[]) {

	// Validate parameters and initialize variables
	if (argc != 2) {
		printf("Wrong argument! Please enter in format: server [ServerPortNumber]");
		return 1;
	}

	WSAData wsaData;
	int iRetVal;
	char buff[BUFF_SIZE];

	short serverPortNumbr = (short) atoi(argv[1]);
	if (serverPortNumbr == 0) {
		printf("Fail to read server port number. Using default value: %d\n", SERVER_PORT_DEFAULT);
		serverPortNumbr = SERVER_PORT_DEFAULT;
	}

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d", iRetVal);
		return 1;
	}

	//Construct socket
	SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// Bind address to socket
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPortNumbr);
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
	if (bind(server, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("Error binding server address. Exiting...");
		closesocket(server);
		WSACleanup();
		return 1;
	}
	printf("\nServer started. Listening...\n\n");

	// Communicate with client
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	
	while (1) {
		// Receive message
		iRetVal = recvfrom(server, buff, MSG_SIZE, 0, (sockaddr*)&clientAddr, &clientAddrLen);
		if (iRetVal == SOCKET_ERROR) {
			printf("Error: %d", WSAGetLastError());
		}
		else if (strlen(buff) > 0) {
			buff[iRetVal] = 0;
			printf("---- Receive from client [%s:%d]: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff);

			processMsg(buff);

			// Log resolve result
			if (buff[0] == DNS_RESOLVE_FAILURE) {
				printf("DNS Resolve failed: Not found information.\n");
			}
			else if (buff[0] == DNS_RESOLVE_SUCCESS) {
				printf("DNS Resolve completed:\n%s\n", buff + 1);
			}

			// Send result to client
			iRetVal = sendto(server, buff, strlen(buff), 0, (sockaddr*)&clientAddr, clientAddrLen);
			if (iRetVal == SOCKET_ERROR) {
				printf("Error: %d", WSAGetLastError());
				continue;
			}
			printf("DNS Response sent to client [%s:%d].", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
		}
		printf("\n\n");
	}

	// Close socket & cleanup
	closesocket(server);
	WSACleanup();

    return 0;
}