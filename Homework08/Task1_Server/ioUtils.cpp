
#include "stdafx.h"
#include "ioUtils.h"

#pragma comment (lib, "Ws2_32.lib")

// Shared variables
FILE* f;
WSAData wsaData;
SOCKET listenSock;

char buff[BUFF_SIZE];
char* fName;

/***********************************
/          NETWORK UTILS           /
***********************************/

int initConnection(short serverPortNumber) {
	int iRetVal;

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d", iRetVal);
		return 1;
	}

	if (serverPortNumber == 0) {
		printf("Cannot identify server port number.");
		WSACleanup();
		return 1;
	}

	// Construct listen socket at non-blocking mode
	unsigned long ul = 1;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ioctlsocket(listenSock, FIONBIO, &ul);

	// Bind address to socket
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPortNumber);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("Error binding server address.\n");
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}

	// Listen request from client
	if (listen(listenSock, 10)) {
		printf("Error %d! Cannot listen.\n", WSAGetLastError());
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}

	printf("Server started!\n");
	return 0;
}

void ioCleanup() {
	closesocket(listenSock);
	WSACleanup();
	if (f != NULL)
		fclose(f);
}

/*
Encode length header, send header & msg to socket s
return 0 if sent successfully, return SOCKET_ERROR if fail to send
[IN] s:			a socket to which the message is sent
[IN] buff:		a char array containing the message to be sent
[IN] len:		an int containing the length of message
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
	if (iRetVal == SOCKET_ERROR) {
		// If error is WSAEWOULDBLOCK, wait 50ms and try again
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			printf("Error sending header %d\n", WSAGetLastError());
			Sleep(50);
			iRetVal = send(s, tempLenBuff, 4, 0);
		}
		if (iRetVal == SOCKET_ERROR)
			return SOCKET_ERROR;
	}

	// Send message
	while (lenLeft > 0) {
		iRetVal = send(s, buff + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				printf("Error sending payload %d\n", WSAGetLastError());
				Sleep(50);
				continue;
			}
			return SOCKET_ERROR;
		}
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	return 0;
}

/*
Receive message from socket s and fill into buffer
return RECVMSG_ERROR if message is larger than bufflen
return 1 if received successfully, return 0 if socket closed connection
return SOCKET_ERROR if fail to receive
[IN] s:			a socket from which the message is received
[OUT] buff:		a char array to store received message
[IN] buffsize:	an int containing the maximum length of char array buff
*/
int recv_s(SOCKET s, char* buff, int buffsize) {
	int iRetVal, lenLeft = 0, idx = 0;

	// Receive 4 bytes and decode length
	iRetVal = recv(s, buff, 4, 0);
	if (iRetVal <= 0) {
		// If error is WSAEWOULDBLOCK, wait 50ms and try again
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			printf("Error receiving header %d\n", WSAGetLastError());
			Sleep(50);
			iRetVal = recv(s, buff, 4, 0);
		}
		if (iRetVal <= 0)
			return SOCKET_ERROR;
	}
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
		if (iRetVal <= 0) {
			printf("Error receiving payload %d\n", WSAGetLastError());
			// If error is WSAEWOULDBLOCK, wait 50ms and try again
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				Sleep(50);
				continue;
			}
			return iRetVal;
		}
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	buff[idx] = 0;
	return 1;
}

int acceptConn(SOCKET* sock) {
	unsigned long ul = 0;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	// accept request
	*sock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
	if (*sock == INVALID_SOCKET) {
		printf("Error accepting! Code: %d\n", WSAGetLastError());
		return 1;
	}
	else {
		printf("Server got connection from client [%s:%d], assigned SOCKET: %d\n",
			inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), *sock);
		return 0;
	}
}

int recvReq(SOCKET sock, char* req, int reqLen) {
	int iRetVal;
	//receive message from client
	iRetVal = recv_s(sock, req, reqLen);
	if (iRetVal == SOCKET_ERROR) {
		printf("Error code %d.\n", WSAGetLastError());
		return 1;
	}
	else if (iRetVal == 0) {
		printf("Client at SOCKET %d closed connection.\n", sock);
		return 1;
	}
	printf("Receive from client at SOCKET %d: %s\n", sock, req);
	return 0;
}

int sendRes(SOCKET sock, char* res, int resultLen) {
	int iRetVal;
	// Send message
	iRetVal = send_s(sock, res, resultLen);

	if (iRetVal == SOCKET_ERROR) {
		printf("[!] Error %d! Cannot send to client at SOCKET %d!\n", WSAGetLastError(), sock);
		return 1;
	}
	return 0;
}


/***********************************
/            FILES UTILS           /
***********************************/

int openFile(char* fileName) {
	int retVal = fopen_s(&f, fileName, "r+");
	fName = fileName;
	if (f == NULL) {
		printf("File does not exist. Creating new file.\n");
		retVal = fopen_s(&f, fileName, "w+");
		if (retVal)
			printf("Cannot create new file.\n");
	}
	printf("Open file successfully.\n");
	return retVal;
}

int readUserF(std::list<Account>& accList) {
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	// Read file
	while (!feof(f)) {
		Account acc;
		fgets(buff, BUFF_SIZE, f);
		
		if (strlen(buff) == 0) continue;
		int i = 0;
		int j = 0;
		//Split user name
		while (buff[i] != ' ') {
			acc.username[j] = buff[i];
			i++;
			j++;
		}
		acc.username[j] = '\0';
		j = 0;
		i++;
		//Split password
		while (buff[i] != ' ') {
			acc.password[j] = buff[i];
			i++;
			j++;
		}
		acc.password[j] = '\0';
		i++;
		// Split status
		acc.isLocked = (buff[i] == '1');
		accList.push_back(acc);
	}
	printf("Data loaded.\n");
	return 0;
}

int writeUserF(std::list<Account> accList) {
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	buff[0] = '\0';

	// Write the first n-1 lines
	std::list<Account>::iterator it = accList.begin();
	std::list<Account>::iterator endIt = accList.end();
	endIt--;
	for (; it != endIt; it++) {
		snprintf(buff, BUFF_SIZE, "%s %s %d\n",
			(*it).username, (*it).password, (*it).isLocked);
		fputs(buff, f);
	}
	// Write last line
	snprintf(buff, BUFF_SIZE, "%s %s %d",
		(*it).username, (*it).password, (*it).isLocked);
	fputs(buff, f);
	// Close file
	fclose(f);
	openFile(fName);
	
	return 0;
}

