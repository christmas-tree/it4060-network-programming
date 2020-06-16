
#include "stdafx.h"
#include "businessUtils.h"
	
int threadCount = 0;
HANDLE sessionListMutex;
HANDLE threadStatus[MAX_THREADCOUNT];

extern SOCKET listenSock;
/*
Function takes a char array and returns 0 if it is a valid IPv4 address
else returns 1
*/
int isHostName(char* userInput) {
	for (int i = 0; userInput[i] != 0; i++) {
		if (isalpha(userInput[i]))
			return 1;
	}
	return 0;
}

/*
Function resolve hostname from IPv4 address and print response to buff,
returns 0 if resolve successfully, returns 1 if fail.
*/
int getName(char* address, char* buff, int buffSize) {
	// Declare variables
	struct in_addr addr;
	hostent* hostInfo;
	char** pAlias;

	// Setup in_addr to pass to gethostbyaddr()
	addr.s_addr = inet_addr(address);
	if (addr.s_addr == INADDR_NONE)
		return 1;

	// Call gethostbyaddr()
	hostInfo = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
	if (hostInfo == NULL) {
		return 1;
	}
	else {
		// Print response to char array outResult
		snprintf(buff, buffSize, "Official name: %s", hostInfo->h_name);
		pAlias = hostInfo->h_aliases;
		if (*pAlias != NULL) {
			strcat_s(buff, buffSize, "\nAllias name(s):");
			while (*pAlias != NULL) {
				strcat_s(buff, buffSize, "\n");
				strcat_s(buff, buffSize, *pAlias);
				pAlias++;
			}
		}
	}
	return 0;
}

/*
Function resolve address from hostname and print response to buff,
returns 0 if resolve successfully, returns 1 if fail.
*/
int getAddress(char* hostName, char* buff, int buffSize) {
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
	dwRetval = getaddrinfo(hostName, "http", &hints, &result);
	if (dwRetval != 0) {
		return 1;
	}

	// Print response to char array outResult
	pResult = result;
	sockaddr = (struct sockaddr_in*) pResult->ai_addr;
	snprintf(buff, buffSize, "Official IP: %s", inet_ntoa(sockaddr->sin_addr));

	pResult = pResult->ai_next;
	if (pResult != NULL) {
		strcat_s(buff, buffSize, "\nAllias IP(s):");
		while (pResult != NULL) {
			sockaddr = (struct sockaddr_in*) pResult->ai_addr;
			strcat_s(buff, buffSize, "\n");
			strcat_s(buff, buffSize, inet_ntoa(sockaddr->sin_addr));
			pResult = pResult->ai_next;
		}
	}
	freeaddrinfo(result);
	return 0;
}

/*
Function determines if buff is hostname or IP Address, then call
corresponding function, construct response and save response to buff
*/
void processRequest(char* buff) {
	int iRetVal;
	char tempBuff[BUFF_SIZE-1];
	// Resolve message
	if (isHostName(buff))
		iRetVal = getAddress(buff, tempBuff, BUFF_SIZE-1);
	else
		iRetVal = getName(buff, tempBuff, BUFF_SIZE-1);
	// Construct response
	if (iRetVal == 1) {
		buff[0] = DNS_RESOLVE_FAILURE;
		buff[1] = 0;
		printf("DNS Resolve failed: Not found information.\n");
	}
	else {
		buff[0] = DNS_RESOLVE_SUCCESS;
		strcpy_s(buff+1, BUFF_SIZE-1, tempBuff);
		printf("DNS Resolve completed:\n%s\n", buff + 1);
	}
}

/*
Thread's main function. Accept new connection and scan sockets
for activities. Call corresponding functions to process message
and send response to client.
*/
unsigned __stdcall procThread(void *threadNumber) {
	// Initialize Variables
	int threadId = *((int*)threadNumber) - 1;
	char buff[BUFF_SIZE];
	SOCKET clientList[FD_SETSIZE] = {0};
	SOCKET connSock;
	fd_set initfds, readfds;
	int iRetVal, nEvents, clientCount = 0;

	// Get control of thread mutex
	WaitForSingleObject(threadStatus[threadId], INFINITE);
	printf("New thread ID %d created.\n", threadId);
	
	FD_ZERO(&initfds);
	FD_SET(listenSock, &initfds);

	while (1) {
		readfds = initfds;
		nEvents = select(0, &readfds, 0, 0, 0);

		if (nEvents < 0) {
			printf("Error %d! Cannot poll sockets.\n", WSAGetLastError());
			break;
		}

		// Check listenSock and accept if thread's capacity is not full
		if (FD_ISSET(listenSock, &readfds)
			&& clientCount < FD_SETSIZE - 1
			&& acceptConn(&connSock) == 0)
		{
			// Store new socket and add socket to initfds
			int i;
			for (i = 0; i < FD_SETSIZE; i++)
				if (clientList[i] == 0) {
					clientList[i] = connSock;
					FD_SET(clientList[i], &initfds);
					// if thread's capacity is full, release thread's mutex
					if (++clientCount == FD_SETSIZE - 1)
						ReleaseMutex(threadStatus[threadId]);
					break;
				}
			if (--nEvents == 0)
				continue;
		}

		// Check for activities in other sockets
		for (int i = 0; i < FD_SETSIZE; i++) {
			if (clientList[i] == 0)
				continue;

			if (FD_ISSET(clientList[i], &readfds)) {
				iRetVal = recvReq(clientList[i], buff, BUFF_SIZE);
				if (iRetVal == 0) {
					processRequest(buff);
					iRetVal = sendRes(clientList[i], buff, strlen(buff));
				}
				if (iRetVal == 1) {
					// Cleanup socket
					FD_CLR(clientList[i], &initfds);
					closesocket(clientList[i]);
					clientList[i] = 0;
					// if thread's capacity is not full, get control of thread's mutex
					if (--clientCount == FD_SETSIZE - 2)
						WaitForSingleObject(threadStatus[threadId], INFINITE);
				}
				if (--nEvents == 0)
					break;
			}
		}
	}
	return 0;
}

void run() {
	// Initialize Mutex
	sessionListMutex = CreateMutex(NULL, false, NULL);

	// Start new thread and create thread's mutex
	threadCount++;
	_beginthreadex(0, 0, procThread, (void*)&threadCount, 0, 0);
	threadStatus[threadCount-1] = CreateMutex(NULL, false, NULL);

	while (1) {
		// main thread sleeps so newly created thread can set up
		Sleep(500);
		if (threadCount <= MAX_THREADCOUNT) {
			// Wait until the mutexes of all threads are released, i.e, all threads are full
			// to create new thread
			WaitForMultipleObjects(threadCount, threadStatus, true, INFINITE);
			threadCount++;
			_beginthreadex(0, 0, procThread, (void*)&threadCount, 0, 0);
			threadStatus[threadCount - 1] = CreateMutex(NULL, false, NULL);

			// Release threads' mutexes
			for (int i = 0; i < threadCount; i++) {
				ReleaseMutex(threadStatus[i]);
			}
		}
		else {
			printf("Maximum capacity reached.\n");
		}
	}
	// Cleanup Handles
	CloseHandle(sessionListMutex);
	for (int i = 0; i < threadCount; i++)
		CloseHandle(threadStatus[i]);
}