
#include "stdafx.h"
#include "businessUtils.h"

/*
Send error message to socket specified
*/
void procErr(SockInfo* newSock) {
	if (sendPacket(newSock, OP_ERR, 0, "")) {
		printf("Cannot send error message to client.\n");
	};
}

/*
Receive file from specified sock, encode/decode
and send processed file back.
*/
void procReq(SockInfo* newSock, bool encode, char* payload) {
	// Declare variables
	char fileName[FILENAME_LEN + 1];
	char fileNameNew[FILENAME_LEN + 5];
	char buff[FILEBUFF_SIZE];
	short iRetVal;
	short key;
	long int bytesRead;

	// Convert payload into short variable key
	key = atoi(payload);

	// Receive file
	iRetVal = recvFile(newSock, fileName);
	if (iRetVal != 0) {
		if (iRetVal == RECVMSG_ERROR)
			procErr(newSock);
		return;
	}

	printf("Running %s on file \'%s\'...\n", encode ? "Encode" : "Decode", fileName);
	// Open received file and create new temp file
	FILE* f = fopen(fileName, "rb");
	snprintf(fileNameNew, FILEBUFF_SIZE+5, "%s.new", fileName);
	FILE* fnew = fopen(fileNameNew, "wb");

	if (f == NULL || fnew == NULL) {
		procErr(newSock);
		return;
	}

	// Process received file and save in new file
	while (bytesRead = fread(buff, 1, FILEBUFF_SIZE, f)) {
		for (int i = 0; i < bytesRead; ++i) {
			if (encode)
				buff[i] = (char) ((short)buff[i] + key) % 256;
			else
				//buff[i] = (buff[i] - key) % 256;
				buff[i] = (char)((short)buff[i] - key) % 256;
		}
		fwrite(buff, 1, bytesRead, fnew);
	}
	fclose(f);
	fclose(fnew);
	printf("%s file \'%s\' completed. Sending to client.\n", encode ? "Encoding" : "Decoding", fileName);

	// Send processed file to specified sock and remove temp files
	iRetVal = sendFile(newSock, fileNameNew);
	remove(fileName);
	remove(fileNameNew);
	if (iRetVal == SOCKET_ERROR) {
		procErr(newSock);
	}
	printf("File \'%s\' is done. Temp files removed.\n", fileName);
}

/*
Extract opcode and call corresponding process function
*/
unsigned __stdcall procNewClient(void* sock) {
	SockInfo* newSock = (SockInfo*)sock;
	int iRetVal;
	short opCode;
	short len;
	char payload[BUFF_SIZE];

	while (1) {
		// Receive request
		iRetVal = recvPacket(newSock, &opCode, &len, payload, BUFF_SIZE-1);
		if (iRetVal != 0) break;

		// Get opcode and call processing function
		switch (opCode) {
		case OP_DEC:
		case OP_ENC:
			procReq(newSock, (OP_ENC==opCode), payload);
			break;
		default:
			procErr(newSock);
		}
	}
	// Close socket and free allocated sock.
	closesocket(newSock->connSock);
	free(newSock);
	return 0;
}

/*
Application driver
*/
void run() {
	while (1) {
		// Initialize a new SockInfo
		SockInfo* newSock = (SockInfo*) malloc(sizeof(SockInfo));
		newSock->clientAddrLen = sizeof(newSock->clientAddr);

		// Accept connection
		int iRetVal = acceptConn(newSock);
		if (iRetVal == 1)
			break;
		_beginthreadex(0, 0, procNewClient, (void*)newSock, 0, 0);
	}
}