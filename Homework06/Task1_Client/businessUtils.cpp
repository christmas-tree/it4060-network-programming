/*
Request format:
1> REQUESTSID
2> REAUTH [sid]
3> LOGIN [sid] [username] [password]
4> LOGOUT [sid]

Response format:
Failure response:		[STATUSCODE]
Success response:		[STATUSCODE] [sid]
*/

#include "stdafx.h"
#include "businessUtils.h"

extern bool isLoggedIn;
char buff[BUFF_LEN];
char sid[SID_LEN];

/*
Parse response in buff. Extract sid if
status code is RES_OK. Return status code
*/
int parseResponse() {
	char strStatusCode[4];
	int statusCode = -1;
	int i, j;

	// Parse status code
	for (i = 0; i < 3; i++) {
		strStatusCode[i] = buff[i];
	}
	strStatusCode[i] = 0;
	statusCode = atoi(strStatusCode);

	// Extract sid
	if (statusCode == RES_OK) {
		j = 0;
		i = 4;
		while (j < SID_LEN) {
			sid[j++] = buff[i++];
		}
	}
	return statusCode;
}

int reqLogIn(char* username, char* password) {
	// Encapsulate request
	snprintf(buff, BUFF_LEN, "LOGIN %s %s %s", sid, username, password);
	if (sendReq(buff, BUFF_LEN) == 1) {
		return 1;
	}

	// Parse response
	int iRetVal = parseResponse();
	if (iRetVal == RES_OK) {
		isLoggedIn = true;
	}
	return iRetVal;
}

int reqLogOut() {
	// Encapsulate request
	snprintf(buff, BUFF_LEN, "LOGOUT %s", sid);
	if (sendReq(buff, BUFF_LEN) == 1) {
		return 1;
	}
	// Parse response
	int iRetVal = parseResponse();
	if (iRetVal == RES_OK) {
		isLoggedIn = false;
	}
	return iRetVal;
}

int reqReauth() {
	// Read SID from file
	readSidFromF(sid, SID_LEN);
	if (strlen(sid) != SID_LEN - 1) {
		return -1;
	} else {
		// Encapsulate reauth message
		snprintf(buff, BUFF_LEN, "REAUTH %s", sid);
		if (sendReq(buff, BUFF_LEN) == 1) {
			return 1;
		}

		// Parse response
		int iRetVal = parseResponse();
		switch (iRetVal) {
		case RES_OK:
			isLoggedIn = true;
			break;
		case RES_RA_FOUND_NOTLI:
			isLoggedIn = false;
			break;
		default:
			isLoggedIn = false;
			sid[0] = 0;		  // Clear saved sid if
			writeSidToF(sid); // sid not found on server
		}
		return iRetVal;
	}
}

int reqSid() {
	// Encapsulate request
	snprintf(buff, BUFF_LEN, "REQUESTSID");
	if (sendReq(buff, BUFF_LEN) == 1) {
		return 1;
	}
	// Parse response in buff
	int iRetVal = parseResponse();
	if (iRetVal == RES_OK) {
		writeSidToF(sid);
	}
	return (iRetVal != RES_OK);
}