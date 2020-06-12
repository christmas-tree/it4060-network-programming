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

#ifndef BUSINESSUTILS_H_
#define BUSINESSUTILS_H_

#include <stdlib.h>
#include <time.h>

#include "fileUtils.h"

#define RES_OK					200
#define RES_BAD_REQUEST			400
#define RES_LI_WRONG_PASS		401
#define RES_LI_ALREADY_LI		402
#define RES_LI_ACC_LOCKED		403
#define RES_LI_ACC_NOT_FOUND	404
#define RES_LI_ELSEWHERE		405
#define RES_LO_NOT_LI			411
#define RES_RA_FOUND_NOTLI		420
#define RES_RA_SID_NOT_FOUND	424

#define TIME_1_DAY				86400
#define TIME_1_HOUR				3600
#define ATTEMPT_LIMIT			3

#define LIREQ_MINLEN			5 + SID_LEN + 1
#define LOREQ_MINLEN			6 + SID_LEN
#define RAREQ_MINLEN			6 + SID_LEN

std::list<Attempt> attemptList;
std::list<Session> sessionList;
std::list<Account> accountList;

HANDLE extern sessionListMutex;

typedef struct {
	LPPER_IO_OPERATION_DATA perIoData;
	LPPER_HANDLE_DATA perHandleData;
} PER_QUEUE_ITEM;

/*
Prepare data. Return 0 if successful, return value > 0 if fail
*/
int initData() {
	return openFile("account.txt") + readUserF(accountList);
}

/*
Generate a new unique SID.
[OUT] sid:		a char array to store newly created SID
*/
void genSid(char* sid) {
	static char* characters = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefjhijklmnopqrstuvwxyz";
	srand((unsigned int)time(NULL));
	bool duplicate = false;
	// Generate new SID until it's unique
	do {
		// Randomly choose a character
		for (int i = 0; i < SID_LEN - 1; i++) {
			sid[i] = characters[rand() % 62];
		}
		sid[SID_LEN - 1] = 0;

		// Check if duplicate
		for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
			if (strcmp((*it).sid, sid) == 0) {
				duplicate = true;
				break;
			}
		}
	} while (duplicate);
}

/*
Find an account in accountList by username, return pointer to account
if account found, return NULL if cannot find.
[IN] username:	a char array that contains the username
to be looked up in accountList
*/
Account* findAccount(char* username) {
	for (std::list<Account>::iterator it = accountList.begin(); it != accountList.end(); it++) {
		if (strcmp(it->username, username) == 0) {
			return &(*it);
		}
	}
	return NULL;
}

/*
Process corrupted message
[OUT] buff:		a char array to store the constructed response message
*/
void processCorruptedMsg(char* buff) {
	printf("Cannot read request. Request corrupted.\n");
	snprintf(buff, BUFSIZE, "%d", RES_BAD_REQUEST);
	return;
}

/*
Process login from sid, username and password and construct response
[IN] sock:		the socket that's sending the request
[OUT] buff:		a char array to store the constructed response message
[IN] sid:		a char array containing the SID to be processed
[IN] username:	a char array containing the username to be processed
[IN] password:	a char array containing the password to be processed
*/
void processLogIn(SOCKET sock, char* buff, char* sid, char* username, char* password) {
	Account* pAccount = NULL;
	Session* pThisSession = NULL;
	time_t now = time(0);

	// Find session
	for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
		if (strcmp(sid, it->sid) == 0) {
			pThisSession = (Session*)&(*it);
			pThisSession->socket = sock;
			break;
		}
	}
	// If session not found, inform bad request
	if (pThisSession == NULL) {
		processCorruptedMsg(buff);
		return;
	}

	WaitForSingleObject(sessionListMutex, INFINITE);
	// Check if session is already associated with an account
	if (pThisSession->pAcc != NULL) {
		printf("An account is already logged in on this device.\n");
		snprintf(buff, BUFSIZE, "%d", RES_LI_ALREADY_LI);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if account exists
	pAccount = findAccount(username);
	if (pAccount == NULL) {
		printf("Account not found!\n");
		snprintf(buff, BUFSIZE, "%d", RES_LI_ACC_NOT_FOUND);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if account is locked
	if (pAccount->isLocked) {
		printf("Account is locked.\n");
		snprintf(buff, BUFSIZE, "%d", RES_LI_ACC_LOCKED);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check password
	if (strcmp(pAccount->password, password) != 0) {
		printf("Wrong password!\n");

		// Check if has attempt before

		for (std::list<Attempt>::iterator it = attemptList.begin(); it != attemptList.end(); it++) {
			if ((*it).pAcc == pAccount) {

				// If last attempt is more than 1 hour before then reset number of attempts
				if (now - (*it).lastAtempt > TIME_1_HOUR)
					(*it).numOfAttempts = 1;
				else
					(*it).numOfAttempts++;
				(*it).lastAtempt = now;

				// If number of attempts exceeds limit then block account and update file 
				if ((*it).numOfAttempts > ATTEMPT_LIMIT) {
					pAccount->isLocked = true;
					writeUserF(accountList);
					printf("Account locked. File updated.\n");
					snprintf(buff, BUFSIZE, "%d", RES_LI_ACC_LOCKED);

					ReleaseMutex(sessionListMutex);
					return;
				}
				snprintf(buff, BUFSIZE, "%d", RES_LI_WRONG_PASS);
				ReleaseMutex(sessionListMutex);
				return;
			}
		}

		// if there is no previous attempt, add attempt
		Attempt newAttempt;
		newAttempt.lastAtempt = now;
		newAttempt.numOfAttempts = 1;
		newAttempt.pAcc = pAccount;
		attemptList.push_back(newAttempt);

		snprintf(buff, BUFSIZE, "%d", RES_LI_WRONG_PASS);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if user has already logged in elsewhere
	for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
		if (((Session*)&(*it) != pThisSession)
			&& (it->pAcc == pAccount)
			&& (it->socket != 0)
			&& (now - it->lastActive < TIME_1_DAY))
		{
			printf("This account is logged in on another socks.\n");
			snprintf(buff, BUFSIZE, "%d", RES_LI_ELSEWHERE);
			ReleaseMutex(sessionListMutex);
			return;
		}
	}

	// Passed all checks. Update active time and session account info
	pThisSession->pAcc = pAccount;
	pThisSession->lastActive = now;
	pThisSession->socket = sock;
	printf("Login successful.\n");
	snprintf(buff, BUFSIZE, "%d %s", RES_OK, sid);

	ReleaseMutex(sessionListMutex);
	return;
}

/*
Process log out and construct response
[OUT] buff:		a char array to store the constructed response message
[IN] sid:		a char array containing the SID to be processed
*/
void processLogOut(char* buff, char* sid) {
	std::list<Session>::iterator it;
	// Find session
	Session* pThisSession = NULL;
	for (it = sessionList.begin(); it != sessionList.end(); it++) {
		if (strcmp(sid, it->sid) == 0) {
			pThisSession = (Session*)&(*it);
			break;
		}
	}
	// If session not found, inform bad request
	if (pThisSession == NULL) {
		processCorruptedMsg(buff);
		return;
	}

	// If there isn't an account attached to session => deny
	if (pThisSession->pAcc == NULL) {
		printf("Account is not logged in. Deny log out.\n");
		snprintf(buff, BUFSIZE, "%d", RES_LO_NOT_LI);
		return;
	}
	// All checks out! Allow log out
	printf("Log out OK.\n");
	snprintf(buff, BUFSIZE, "%d %s", RES_OK, pThisSession->sid);
	pThisSession->pAcc = NULL;
	printf("Log out successful.\n");
	return;
}

/*
Process reauthentication request and construct response
[IN] sock:		the socket that's sending the request
[OUT] buff:		a char array to store the constructed response message
[IN] sid:		a char array containing the SID to be processed
*/
void processReauth(SOCKET sock, char* buff, char* sid) {
	time_t now = time(0);
	std::list<Session>::iterator it;
	Session* pThisSession = NULL;

	// Find session
	for (it = sessionList.begin(); it != sessionList.end(); it++)
		if (strcmp(sid, it->sid) == 0) {
			pThisSession = (Session*)&(*it);
			break;
		}
	// Session do not exists
	if (pThisSession == NULL) {
		printf("Session not found.\n");
		snprintf(buff, BUFSIZE, "%d", RES_RA_SID_NOT_FOUND);
		return;
	}
	// Check if there's an account attached to session
	if (pThisSession->pAcc == NULL) {
		printf("Session exists but not logged in..\n");
		snprintf(buff, BUFSIZE, "%d", RES_RA_FOUND_NOTLI);
		return;
	}
	// Check if lastActive is more than 1 day
	if (now - pThisSession->lastActive > TIME_1_DAY) {
		printf("Login session timeout. Deny reauth.\n");
		pThisSession->pAcc = NULL;
		snprintf(buff, BUFSIZE, "%d", RES_RA_FOUND_NOTLI);
		return;
	}
	// Check if account is disabled
	if (pThisSession->pAcc->isLocked) {
		printf("Account is locked. Reauth failed.\n");
		pThisSession->pAcc = NULL;
		snprintf(buff, BUFSIZE, "%d", RES_LI_ACC_LOCKED);
		return;
	}
	// Check if account is logged in on another device
	WaitForSingleObject(sessionListMutex, INFINITE);
	for (it = sessionList.begin(); it != sessionList.end(); it++) {
		if (((Session*)&(*it) != pThisSession)
			&& (it->pAcc == pThisSession->pAcc)
			&& (it->socket != 0)
			&& (now - it->lastActive < TIME_1_DAY))
		{
			printf("This account is logged in on another socks.\n");
			pThisSession->pAcc = NULL;
			snprintf(buff, BUFSIZE, "%d", RES_LI_ELSEWHERE);
			ReleaseMutex(sessionListMutex);
			return;
		}
	}
	// All checks out!
	printf("Session exists. Allow reauth.\n");
	pThisSession->lastActive = time(0);
	pThisSession->socket = sock;
	snprintf(buff, BUFSIZE, "%d %s", RES_OK, pThisSession->sid);
	ReleaseMutex(sessionListMutex);
	return;
}

/*
Process sid request and construct response
[IN] sock:		the socket that's sending the request
[OUT] buff:		a char array to store the constructed response message
[OUT] responseLen: a short to store the length of the response payload
*/
void processRequestSid(SOCKET sock, char* buff) {
	char sid[SID_LEN];

	WaitForSingleObject(sessionListMutex, INFINITE);
	genSid(sid);

	// Create new session and add to sessionList
	Session newSession;
	newSession.pAcc = NULL;
	newSession.lastActive = time(0);
	newSession.socket = sock;
	strcpy_s(newSession.sid, SID_LEN, sid);
	sessionList.push_back(newSession);

	// Construct response
	snprintf(buff, BUFSIZE, "%d %s", RES_OK, newSession.sid);
	ReleaseMutex(sessionListMutex);
	printf("Generated new SID for socks: %s.\n", sid);
}

/*
Extract Sid from req and the provided index, return 0 if Sid is valid, else return 1
[IN] buff:		a char array containing the original request
[OUT] sid:		a char array to store extracted SID
[IN/OUT] i:		an int which is the index of the first character of SID in buff.
When function returns, value will be updated to the index of the character
after the last character of SID.
*/
int extractSid(char* buff, char* sid, int &i) {
	int j = 0;
	i++;
	while (j < SID_LEN - 1) {
		if (buff[i] == ' ') {
			sid[0] = 0;
			return 1;
		}
		sid[j++] = buff[i++];
	}
	sid[j] = 0;
	return 0;
}

/*
Extract command from request and call corresponding process function
[IN] sock:		the socket that's sending the request
[IN/OUT] buff:	a char array which contains the request, and stores response
after the request has been processed
*/
void parseRequest(SOCKET sock, char* buff) {
	char command[20];
	char sid[SID_LEN];

	// Extract command
	int i, j;
	for (i = 0; buff[i] != ' ' && buff[i] != 0; i++) {
		command[i] = buff[i];
	}
	command[i] = 0;

	// Execute command
	if (_stricmp(command, "LOGIN") == 0) {
		// Validate request and extract SID
		if (strlen(buff) < LIREQ_MINLEN) {
			processCorruptedMsg(buff);
			return;
		}
		if (extractSid(buff, sid, i)) {
			processCorruptedMsg(buff);
			return;
		}
		// Extract username and password
		char username[CRE_MAXLEN];
		char password[CRE_MAXLEN];

		j = 0;
		for (i = i + 1; buff[i] != ' ' && buff[i] != 0; i++) {
			username[j++] = buff[i];
		}
		username[j] = 0;

		j = 0;
		for (i = i + 1; buff[i] != ' ' && buff[i] != 0; i++) {
			password[j++] = buff[i];
		}
		password[j] = 0;

		// Process Login
		processLogIn(sock, buff, sid, username, password);
	}
	else if (_stricmp(command, "LOGOUT") == 0) {
		// Validate request and extract SID
		if (strlen(buff) < LOREQ_MINLEN) {
			processCorruptedMsg(buff);
			return;
		}
		if (extractSid(buff, sid, i)) {
			processCorruptedMsg(buff);
			return;
		}
		// Process Login
		processLogOut(buff, sid);
	}
	else if (_stricmp(command, "REAUTH") == 0) {
		// Validate request and extract SID
		if (strlen(buff) < RAREQ_MINLEN) {
			processCorruptedMsg(buff);
			return;
		}
		if (extractSid(buff, sid, i)) {
			processCorruptedMsg(buff);
			return;
		}
		// Process Login
		processReauth(sock, buff, sid);
	}
	else if (_stricmp(command, "REQUESTSID") == 0) {
		// Process Sid request
		processRequestSid(sock, buff);
	}
	else {
		processCorruptedMsg(buff);
	}
}

/*
Remove socket information from any Session previously associated with it
[IN] sock:	the socket which has been disconnected
*/
void disconnect(SOCKET sock) {
	for (auto it = sessionList.begin(); it != sessionList.end(); it++) {
		if (it->socket == sock)
			it->socket = 0;
	}
}

#endif