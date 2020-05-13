#include "stdafx.h"
#include "businessUtils.h"

char buff[BUFF_SIZE];

/*
Send and receive request and file to and from server.
return 1 if operation fails, else return 0.
*/
int procReq(bool encode, char* filePath, char* key) {
	int iRetVal;

	// Send request, including character '\0' to indicate this is a string
	iRetVal = sendPacket(encode ? OP_ENC : OP_DEC, (short) strlen(key) + 1, key);
	if (iRetVal == 1) {
		return 1;
	}
	// Send file
	iRetVal = sendFile(filePath);
	if (iRetVal == 1) {
		return 1;
	}

	// If request to encode file, add extension .enc to new file's name
	if (encode) {
		strcat_s(filePath, FILENAME_SIZE, ".enc");
	}
	else {
		// Else remove extension .enc from file name if exists
		int fileNameLen = strlen(filePath);
		int i;
		for (i = fileNameLen - 1; i >= 0 && filePath[i] != '.'; i--);
		if (i > 0 && strcmp(filePath+i, ".enc") == 0)
			filePath[i] = '\0';
	}

	// Receive processed file from server
	iRetVal = recvFile(filePath);
	if (iRetVal == 1) {
		return 1;
	}
	return 0;
}