#pragma once

#ifndef BUSINESS_UTILS_H_
#define BUSINESS_UTILS_H_
#include <stdlib.h>
#include <process.h>

#include "ioUtils.h"

#define OP_FILENAME			0
#define OP_FILECONTENT		1
#define OP_FILEDIGEST		2
#define OP_ERROR			3
#define	OP_OK				4

#define ERROR_SERVER_FAIL	"500"
#define	ERROR_BAD_REQUEST	"400"
#define	ERROR_FILE_CORRUPTED "450"

MD5 md5;

/*
Function takes opCode and status received from server and print
corresponding message to screen. Function returns 1 if it is a
critical error that needs to shut down the program. Returns 0
if it is successful. Returns 1 if it is an error but does not
need to stop program.
[IN] opCode:	the opCode received
[IN] buff:		the status code received
*/
int decodeReturnCode(int opCode, char* buff) {
	switch (opCode) {
	case OP_ERROR:
		if (strcmp(buff, ERROR_FILE_CORRUPTED) == 0) {
			printf("Server returned error %s! Resending file...\n", ERROR_FILE_CORRUPTED);
			return -1;
		}
		else if (strcmp(buff, ERROR_SERVER_FAIL) == 0) {
			printf("Server returned error %s! Internal server error!\n", ERROR_SERVER_FAIL);
			return 1;
		}
		else if (strcmp(buff, ERROR_BAD_REQUEST) == 0) {
			printf("Server returned error %s! Bad request!\n", ERROR_BAD_REQUEST);
			return 1;
		}
		else {
			printf("Server returned unknown error %s!\n", buff);
			return 1;
		}
		break;
	case OP_OK:
		printf("Server returned success code.\n");
		return 0;
	default:
		printf("Unknown response from server!\n");
		return 1;
	}
}

/*
Main function for a file sending thread. Function creates a new connection
to server, send file until file is sent successfully.
[IN] passedFilePath:	a pointer to a char array containing the path
						of the file to be sent to the server.
*/
unsigned __stdcall clientWorkerThread(LPVOID passedFilePath) {
	int iRetVal, readSize;
	int offset, opCode, payloadLength;
	char buff[PAYLOAD_SIZE];
	char *digest, *fileName;
	char *filePath = (char*)passedFilePath;
	FILE *f;
	SOCKET outSock;

	// Extract file name from file path
	fileName = strrchr(filePath, '/');
	if (fileName == NULL)
		fileName = filePath;

	// Create MD5 hash
	digest = md5.digestFile(filePath);
	printf("MD5 Hash: %s\n", digest);
	
	// Connect to server
	if (connectToServer(&outSock) == 1) return 1;

	// Send file name to server
	if (sendMessage(outSock, OP_FILENAME, strlen(fileName) + 1, 0, fileName) != 0) {
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 1;
	}

	// Receive confirmation from server
	if (recvMessage(outSock, &opCode, &payloadLength, &offset, buff)) {
		printf("Error %d while receiving server confirmation.\n", WSAGetLastError());
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 1;
	}
	if (decodeReturnCode(opCode, buff)) {
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 0;
	}

	// Send file digest to server
	if (sendMessage(outSock, OP_FILEDIGEST, strlen(digest) + 1, 0, digest) != 0) {
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 1;
	}

	// Receive confirmation from server
	if (recvMessage(outSock, &opCode, &payloadLength, &offset, buff)) {
		printf("Error %d while receiving server confirmation.\n", WSAGetLastError());
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 1;
	}
	if (decodeReturnCode(opCode, buff)) {
		printf("closing socket %d\n", outSock);
		closesocket(outSock);
		return 0;
	}

	// Transfer file until success
	while (1) {
		offset = 0;
		readSize = -1;

		// Open file. If file not exists, exit thread
		f = fopen(fileName, "rb");
		if (f == NULL) {
			printf("Cannot open file specified file!\n");
			printf("closing socket %d\n", outSock);
			closesocket(outSock);
			return 1;
		}

		// Read and send file parts until EOF
		while (readSize != 0) {
			readSize = (short)fread(buff, 1, PAYLOAD_SIZE, f);
			if (sendMessage(outSock, OP_FILECONTENT, readSize, offset, buff)) {
				printf("Error %d while sending file content.\n", WSAGetLastError());
				fclose(f);
				printf("closing socket %d\n", outSock);
				closesocket(outSock);
				return 1;
			}
			printf("Sent offset %d", offset);
			offset += readSize;
		}
		fclose(f);

		// Receive confirmation from server
		if (recvMessage(outSock, &opCode, &payloadLength, &offset, buff)) {
			printf("Error %d while receiving server confirmation.\n", WSAGetLastError());
			printf("closing socket %d\n", outSock);
			closesocket(outSock);
			return 1;
		}
		iRetVal = decodeReturnCode(opCode, buff);
		if (iRetVal == -1) {
			continue;
		}
		else {
			printf("closing socket %d\n", outSock);
			closesocket(outSock);
			return 0;
		}
	}
}

/*
Create a new thread to send file to server.
[IN] passedFilePath:	a pointer to a char array containing the path
						of the file to be sent to the server.
*/
void sendFile(char* filePath) {
	if (_beginthreadex(0, 0, clientWorkerThread, (void*)filePath, 0, 0) == 0)
		printf("Create worker thread failed with error %d\n", GetLastError());
	else
		printf("New worker thread created to send file %s\n", filePath);
}

#endif // !BUSINESS_UTILS_H_