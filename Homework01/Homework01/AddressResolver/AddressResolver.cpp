
#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#undef UNICODE

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <conio.h>

// link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

int isHostName(char* userInput) {
	for (int i = 0; userInput[i] != 0; i++) {
		if (isalpha(userInput[i]))
			return 1;
	}
	return 0;
}

int getAddress(char* userInput) {
	// Declare variables
	struct in_addr addr;
	hostent* hostInfo;
	char** pAlias;

	// Setup in_addr to pass to gethostbyaddr()
	addr.s_addr = inet_addr(userInput);
	if (addr.s_addr == INADDR_NONE) {
		printf("Not found information");
		WSACleanup();
		return 1;
	}

	// Call gethostbyaddr()
	hostInfo = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
	if (hostInfo == NULL) {
		printf("Not found information");
		WSACleanup();
		return 1;
	}
	else {
		// Print out hostname and aliases
		printf("List of names:\n");
		printf(hostInfo->h_name);
		for (pAlias = hostInfo->h_aliases; *pAlias != NULL; pAlias++) {
			printf("\n");
			printf(*pAlias);
		}
	}
	return 0;
}

int getName(char* userInput) {
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
		printf("Not found information");
		WSACleanup();
		return 1;
	}

	// Print out retrieved IP addresses
	pResult = result;
	printf("List of IPs:\n");
	while (pResult != NULL) {
		sockaddr = (struct sockaddr_in*) pResult->ai_addr;
		printf(inet_ntoa(sockaddr->sin_addr));
		printf("\n");
		pResult = pResult->ai_next;
	}

	freeaddrinfo(result);
	WSACleanup();
	return 0;
}

int main(int argc, char* argv[])
{
	// -------------------------------------
	// Validating arguments
	if (argc < 2) {
		printf("IP Address or Hostname not entered.");
		return 1;
	}
	if (argc > 2) {
		printf("Too many arguments.");
		return 1;
	}

	// Declare and initialize Winsock
	char* userInput = argv[1];
	WSAData wsaData;
	int iResult;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed. Error code: %d", iResult);
		return 1;
	}

	// Check if user input is a string with alphabet characters.
	// If yes, attempt to resolve IP Address from hostname. If no, find hostname from IP address.
	if (isHostName(userInput)) {
		return getName(userInput);
	}
	else {
		return getAddress(userInput);
	}
}

// TRAN TRUNG NGHIA - 20173281