
#include "stdafx.h"
#include "ioUtils.h"

#pragma comment (lib, "Ws2_32.lib")

// Shared variables
WSAData wsaData;
SOCKET listenSock;

/*
Initialize network environment, return 0
if initialize successful else return 1
*/
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

	/*
	// Construct socket
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	*/

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

/*
Close socket and cleanup Winsock
*/
void networkCleanup() {
	closesocket(listenSock);
	WSACleanup();
}

/*
Accept connection and save socket info to 'sock'.
return 1 if fail to accept, return 0 if succeed
*/
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
		//ioctlsocket(*sock, FIONBIO, &ul);
		printf("Server got connection from clientList [%s:%d], assigned SOCKET: %d\n",
			inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), *sock);
		return 0;
	}
}

/*
Construct packet and send to the address in sock
return 0 if send successfully
return SOCKET_ERROR if fail to send
*/
int sendPacket(SOCKET sock, short opCode, short len, char* payload) {
	short iRetVal, lenLeft = len, idx = 0;
	char buff[4];

	// Send 3 bytes
	buff[0] = (char) opCode;
	buff[1] = (len >> 8) & 0xFF;
	buff[2] = len & 0xFF;

	iRetVal = send(sock, buff, 3, 0);
	if (iRetVal == SOCKET_ERROR) {
		printf("[!] Error %d! Cannot send to client!\n", WSAGetLastError());
		return SOCKET_ERROR;
	}

	// Send message
	while (lenLeft > 0) {
		iRetVal = send(sock, payload + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR)
			return SOCKET_ERROR;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	return 0;
}

/*
Receive a packet from sock and extract opcode, len and payload.
if received payload is larger than payloadMaxlen, discard packet
and return RECVMSG_ERROR.
return 0 if received successfully, return 1 if socket closed connection
return SOCKET_ERROR if fail to receive
*/
int recvPacket(SOCKET sock, short* opCode, short* len, char* payload, short payloadMaxLen) {
	short iRetVal, lenLeft = 0, idx = 0;
	bool discard = false;
	char buff[4];

	// Receive 3 bytes
	iRetVal = recv(sock, buff, 3, 0);
	if (iRetVal == SOCKET_ERROR) {
		printf("Error code %d.\n", WSAGetLastError());
		return -1;
	}
	else if (iRetVal == 0) {
		printf("Client at socket %d closed connection.\n", sock);
		return 1;
	}

	// Extract opcode and len
	*opCode = (short) buff[0];
	*len = ((buff[1] << 8) & 0xFFFF) | ((buff[2]) & 0xFF);

	// If len received is larger than payloadMaxLen then discard packet
	if (*len > payloadMaxLen) {
		lenLeft = *len;
		while (lenLeft > 0) {
			iRetVal = recv(sock, payload,
				(lenLeft > payloadMaxLen) ? payloadMaxLen : lenLeft, 0);
			if (iRetVal <= 0)
				return iRetVal;
			lenLeft -= iRetVal;
		}
		return RECVMSG_ERROR;
	}

	// Receive payload
	lenLeft = *len;
	while (lenLeft > 0) {
		iRetVal = recv(sock, payload + idx, lenLeft, 0);
		if (iRetVal <= 0)
			return iRetVal;
		lenLeft -= iRetVal;
		idx += iRetVal;
	}
	return 0;
}

/*
Receive a file from sock and save file using a unique name
return 0 if received successfully, return 1 if socket closed connection
return SOCKET_ERROR if fail to receive
return RECVMSG_ERROR if received payload is larger than buff or file is
not fully transferred.

int recvFile(SOCKET sock, char* fileName) {
	// Declare variables
	char buff[FILEBUFF_SIZE];
	FILE* f;
	bool end = false;
	short iRetVal;
	short opCode;
	short len;
	short part = 0;

	// Generate random unique file name
	genRandName(fileName);
	while (f = fopen(fileName, "rb")) {
		fclose(f);
		genRandName(fileName);
	};

	f = fopen(fileName, "wb");
	// Loop receive until receive a packet with OP_FILE and len 0
	while (!end) {
		iRetVal = recvPacket(sock, &opCode, &len, buff, FILEBUFF_SIZE);
		if (iRetVal != 0) {
			fclose(f);
			remove(fileName);
			return iRetVal;
		}
		// If OP_FILE then continue loop, else break
		if (opCode == OP_FILE) {	
			// File properly transferred
			if (len == 0) {
				printf("Receive from client at socket %d: EOF\n", sock);
				printf("File saved as \'%s\'.\n", fileName);
				end = true;
			}
			// Miss a part
			else {
				printf("Receive from client at socket %d: Partition %d\n", sock, ++part);
				fwrite(buff, 1, len, f);
			}
		}
		else break;
	}

	// File is not fully transferred.
	if (!end) {
		fclose(f);
		remove(fileName);
		printf("File from client %d was not fully transferred but server received another request.\n", sock);
		return RECVMSG_ERROR;
	}
	fclose(f);
	return 0;
}
*/

/*
Open file indicated by fileName and send to sock
return 0 if send successful, return 1 if fail
*/
int sendFile(SOCKET sock, char* fileName) {
	char buff[FILEBUFF_SIZE];
	FILE* f;
	short iRetVal;
	short readSize;

	// Open file, if file not exists return 1
	f = fopen(fileName, "rb");
	if (f == NULL)
		return 1;

	// Read and send file parts until EOF
	while (1) {
		readSize = (short) fread(buff, 1, PART_SIZE, f);

		iRetVal = sendPacket(sock, OP_FILE, readSize, buff);
		if (iRetVal == SOCKET_ERROR) {
			printf("Error code %d.\n", WSAGetLastError());
			fclose(f);
			remove(fileName);
			return 1;
		}
		if (readSize == 0) break;
	}
	fclose(f);
	return 0;
}