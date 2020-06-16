
#include "stdafx.h"

#include "ioUtils.h"

#define BUFF_SIZE 100
#define RECVMSG_CORRUPT 2
#define SERVER_ADDR "127.0.0.1"

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

/*
Initialize network environment, return 0
if initialize successful else return 1
*/
int initConnection(short serverPortNumbr) {
	int iRetVal;

	// Initialize Winsock
	iRetVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iRetVal != 0) {
		printf("WSAStartup failed. Error code: %d", iRetVal);
		return 1;
	}

	if (serverPortNumbr == 0) {
		printf("Cannot identify server port number.");
		WSACleanup();
		return 1;
	}

	// Construct socket
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Bind address to socket
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPortNumbr);
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

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
			iRetVal = recv(s, buff, buffsize, 0);
			if (iRetVal <= 0)
				return iRetVal;
			lenLeft -= iRetVal;
		}
		buff[0] = 0;
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

/*
Accept connection. Return 1 if fail to accept
return 0 if succeed
*/
int acceptConn(SockInfo* sock) {
	//ZeroMemory(&(sock->clientAddr), sock->clientAddrLen);

	// accept request
	sock->connSock = accept(listenSock, (sockaddr *)&(sock->clientAddr), &(sock->clientAddrLen));
	if (sock->connSock == INVALID_SOCKET) {
		printf("Error accepting! Code: %d\n", WSAGetLastError());
		return 1;
	}
	else {
		printf("Server got connection from client [%s:%d].\n",
			inet_ntoa(sock->clientAddr.sin_addr), ntohs(sock->clientAddr.sin_port));
		return 0;
	}
}

/*
Receive request from specified socket.
return 1 if fail to receive, return 0 if succeed
*/
int recvReq(SockInfo* sock, char* req, int reqLen) {
	int iRetVal;
	//receive message from client
	iRetVal = recv_s(sock->connSock, req, reqLen);
	if (iRetVal == SOCKET_ERROR) {
		printf("Error code %d.\n", WSAGetLastError());
		return 1;
	}
	else if (iRetVal == 0) {
		printf("Client [%s:%d] closed connection.\n",
			inet_ntoa(sock->clientAddr.sin_addr), ntohs(sock->clientAddr.sin_port));
		return 1;
	}
	printf("Receive from client [%s:%d] %s\n",
		inet_ntoa(sock->clientAddr.sin_addr), ntohs(sock->clientAddr.sin_port), req);
	return 0;
}

/*
Send response to client, return 1 if fail to send
return 0 if succeed
*/
int sendRes(SockInfo* sock, char* res, int resultLen) {
	int iRetVal;
	// Send message
	iRetVal = send_s(sock->connSock, res, resultLen);

	if (iRetVal == SOCKET_ERROR) {
		printf("[!] Error %d! Cannot send to client!\n", WSAGetLastError());
		return 1;
	}
	return 0;
}


/***********************************
/            FILES UTILS           /
***********************************/

/*
Open file and store file name in fname. If file does not exists,
create new file. Return 0 if succeed, return an error code
if fail.
*/
int openFile(char* fileName) {
	int retVal = fopen_s(&f, fileName, "r+");
	fName = fileName;
	if (f == NULL) {
		printf("File does not exist. Creating new file.\n");
		int retVal = fopen_s(&f, fileName, "w+");
		if (retVal)
			printf("Cannot create new file.\n");
	}
	printf("Open file successfully.\n");
	return retVal;
}

/*
Read account data from opened file, init data structure
and add to accList. Return 1 if fail, return 0 if succeed
*/
int readUserF(std::list<Account>& accList) {
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	// Read file
	while (!feof(f)) {
		Account acc;
		fgets(buff, BUFF_SIZE, f);
		
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

/*
Stringify account data and write to file.
Return 1 if fail, return 0 if succeed
*/
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

