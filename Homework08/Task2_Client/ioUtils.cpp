
#include "stdafx.h"
#include "ioUtils.h"

#pragma comment (lib, "Ws2_32.lib")

// Shared variables
WSAData wsaData;
SOCKET outSock;
sockaddr_in serverAddr;
MD5 md5;

/*
Initialize network environment, return 0
if initialize successful else return 1
*/
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
	outSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int tv = 10000;
	//setsockopt(outSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	// Request to connect to server
	iRetVal = connect(outSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (iRetVal == SOCKET_ERROR) {
		printf("Error %d! Cannot connect to server.", WSAGetLastError());
		networkCleanup();
		return 1;
	}

	return 0;
}

/*
Close socket and cleanup Winsock
*/
void networkCleanup() {
	closesocket(outSock);
	WSACleanup();
}

/*
Construct packet and send to the address in sock
return 0 if send successfully
return SOCKET_ERROR if fail to send
*/
int sendPacket(short opCode, short len, char* payload) {
	short iRetVal, lenLeft = len, idx = 0;
	char buff[4];

	// Send 3 bytes
	buff[0] = (char) opCode;
	buff[1] = (len >> 8) & 0xFF;
	buff[2] = len & 0xFF;

	iRetVal = send(outSock, buff, 3, 0);
	if (iRetVal == SOCKET_ERROR) {
		printf("[!] Error %d! Cannot send to server!\n", WSAGetLastError());
		return SOCKET_ERROR;
	}

	// Send message
	while (lenLeft > 0) {
		iRetVal = send(outSock, payload + idx, lenLeft, 0);
		if (iRetVal == SOCKET_ERROR) {
			printf("[!] Error %d! Cannot send to server!\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
		lenLeft -= iRetVal;
		idx += iRetVal;
		printf("Sent %d bytes.\n", iRetVal);
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
int recvPacket(short* opCode, short* len, char* payload, short payloadMaxLen) {
	short iRetVal, lenLeft = 0, idx = 0;
	bool discard = false;
	char buff[4];

	// Receive 3 bytes
	iRetVal = recv(outSock, buff, 3, 0);
	if (iRetVal == SOCKET_ERROR) {
		printf("Error code %d while receiving.\n", WSAGetLastError());
		return -1;
	}
	else if (iRetVal == 0) {
		printf("Server closed connection.\n");
		return 1;
	}

	// Extract opcode and len
	*opCode = (short)buff[0];
	*len = ((buff[1] << 8) & 0xFFFF) | ((buff[2]) & 0xFF);

	// If len received is larger than payloadMaxLen then discard packet
	if (*len > payloadMaxLen) {
		lenLeft = *len;
		while (lenLeft > 0) {
			iRetVal = recv(outSock, payload,
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
		iRetVal = recv(outSock, payload + idx, lenLeft, 0);
		if (iRetVal <= 0) {
			printf("Error code %d while receiving.\n", WSAGetLastError());
			return iRetVal;
		}
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
*/
int recvFile(char* fileName) {
	// Declare variables
	char buff[FILEBUFF_SIZE];
	FILE* f;
	bool end = false;
	bool success = false;
	short iRetVal;
	short opCode;
	short len;
	int part = 0;

		// Check if a file with name 'fileName' exists
		while (f = fopen(fileName, "rb")) {
			fclose(f);
			// Extract file extension if exists
			char fileExt[10];
			int fileNameLen = strlen(fileName);
			int i;
			for (i = fileNameLen - 1; i >= 0 && fileName[i] != '.'; i--);
			strcpy_s(fileExt, 10, fileName + i);
			// Add '0' to the part before file extension
			if (i > 0)
				snprintf(fileName + i, FILENAME_SIZE, "0%s", fileExt);
			else
				strcat_s(fileName, FILENAME_SIZE, "0");
		};

	while (!success) {
		// Check if a file with name fileName can be written
		if ((f = fopen(fileName, "wb")) == NULL) {
			printf("Error! Cannot write file %s\n", fileName);
			return -1;
		};
		// Loop receive until receive a packet with OP_FILE and len 0
		while (!end) {
			iRetVal = recvPacket(&opCode, &len, buff, FILEBUFF_SIZE);
			if (iRetVal != 0) {
				fclose(f);
				remove(fileName);
				return iRetVal;
			}
			// If OP_FILE then continue loop, else break
			if (opCode == OP_FILE) {
				// File properly transferred
				if (len == 0) {
					printf("Receive from server: EOF\n");
					end = true;
				}
				// Miss a part
				else {
					printf("Receive from server Part %d\n", ++part);
					fwrite(buff, 1, len, f);
				}
			}
			else break;
		}
		fclose(f);

		// File is not fully transferred.
		if (!end) {
			remove(fileName);
			printf("File from server was not fully transferred.\n");
			return RECVMSG_ERROR;
		}

		// Receive MD5
		iRetVal = recvPacket(&opCode, &len, buff, DIGEST_SIZE);

		if (strcmp(md5.digestFile(fileName), buff) == 0) {
			printf("File integrity verified.\n");
			sendPacket(OP_FILE, 0, "");
			success = true;
		}
		else {
			printf("File corrupted. Asking server to send file again.\n");
			sendPacket(OP_ERR, 0, "");
			remove(fileName);
			part = 0;
		}
	}
	return 0;

}


/*
Open file indicated by fileName and send to sock
return 0 if send successful, return 1 if fail
*/
int sendFile(char* fileName) {
	char buff[FILEBUFF_SIZE];
	char digest[DIGEST_SIZE];
	FILE* f;
	short iRetVal;
	short readSize = -1;
	bool success = false;

	// Open file, if file not exists return 1
	f = fopen(fileName, "rb");
	if (f == NULL)
		return 1;

	while (!success) {
		// Read and send file parts until EOF
		while (readSize != 0) {
			readSize = (short)fread(buff, 1, PART_SIZE, f);

			iRetVal = sendPacket(OP_FILE, readSize, buff);
			if (iRetVal == SOCKET_ERROR) {
				printf("Error code %d.\n", WSAGetLastError());
				fclose(f);
				return 1;
			}
		}
		fclose(f);

		strcpy(digest, md5.digestFile(fileName));
		printf("MD5 Hash: %s\n", digest);
		// Send MD5 code
		iRetVal = sendPacket(OP_FILE, DIGEST_SIZE, digest);
		if (iRetVal == SOCKET_ERROR) {
			printf("Error code %d while sending file.\n", WSAGetLastError());
			return 1;
		}
		
		short opCode;
		short len;
		iRetVal = recvPacket(&opCode, &len, buff, FILEBUFF_SIZE);
		if (opCode == OP_FILE && len == 0) {
			printf("Server received file successfully.\n");
			success = true;
		}
		else if (opCode == OP_ERR && len == 0) {
			printf("Server did not receive file successfully.\n");
			continue;
		}
		else {
			printf("Unknown return value from server.\n");
			return 1;
		}
	}
	return 0;
}