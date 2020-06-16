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
#include <time.h>

#include "businessUtils.h"
	
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

#define BUFF_LEN 1024

#define LIREQ_MINLEN			5 + SID_LEN + 1
#define LOREQ_MINLEN			6 + SID_LEN
#define RAREQ_MINLEN			6 + SID_LEN

std::list<Attempt> attemptList;
std::list<Session> sessionList;
std::list<Account> accountList;

HANDLE sessionListMutex;

/*
Prepare data. Return 0 if successful,
return value > 0 if fail
*/
int initData() {
	return openFile("account.txt") + readUserF(accountList);
}

/*
Generate a new unique SID
*/
void genSid(char* sid) {
	static char* characters = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefjhijklmnopqrstuvwxyz";
	srand((unsigned int) time(NULL));
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
Find an account in accountList by username
return pointer to account if account found
return NULL if cannot find
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
*/
void processCorruptedMsg(char* buff) {
	printf("Cannot read request. Request corrupted.\n");
	snprintf(buff, BUFF_LEN, "%d", RES_BAD_REQUEST);
	return;
}

/*
Process login from sid, username and password
and encapsulate response
*/
void processLogIn(char* buff, char* sid, char* username, char* password) {
	Account* pAccount = NULL;
	Session* pThisSession = NULL;
	time_t now = time(0);

	// Find session
	for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
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

	WaitForSingleObject(sessionListMutex, INFINITE);
	// Check if session is already associated with an account
	if (pThisSession->pAcc != NULL) {
		printf("An account is already logged in on this client.\n");
		snprintf(buff, BUFF_LEN, "%d", RES_LI_ALREADY_LI);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if account exists
	pAccount = findAccount(username);
	if (pAccount == NULL) {
		printf("Account not found!\n");
		snprintf(buff, BUFF_LEN, "%d", RES_LI_ACC_NOT_FOUND);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if account is locked

	if (pAccount->isLocked) {
		printf("Account is locked.\n");
		snprintf(buff, BUFF_LEN, "%d", RES_LI_ACC_LOCKED);
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
					snprintf(buff, BUFF_LEN, "%d", RES_LI_ACC_LOCKED);

					ReleaseMutex(sessionListMutex);
					return;
				}
				snprintf(buff, BUFF_LEN, "%d", RES_LI_WRONG_PASS);
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

		snprintf(buff, BUFF_LEN, "%d", RES_LI_WRONG_PASS);
		ReleaseMutex(sessionListMutex);
		return;
	}

	// Check if user has already logged in elsewhere
	for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
		if (((Session*)&(*it) != pThisSession)
			&& (it->pAcc == pAccount)
			&& (now - it->lastActive < TIME_1_DAY))
		{
			printf("This account is logged in on another client.\n");
			snprintf(buff, BUFF_LEN, "%d", RES_LI_ELSEWHERE);
			ReleaseMutex(sessionListMutex);
			return;
		}
	}

	// Passed all checks. Update active time and session account info
	pThisSession->pAcc = pAccount;
	pThisSession->lastActive = now;
	printf("Login successful.\n");
	snprintf(buff, BUFF_LEN, "%d %s", RES_OK, sid);

	ReleaseMutex(sessionListMutex);
	return;
}

/*
Process log out and encapsulate response
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
		snprintf(buff, BUFF_LEN, "%d", RES_LO_NOT_LI);
		return;
	}
	// All checks out! Allow log out
	printf("Log out OK.\n");
	snprintf(buff, BUFF_LEN, "%d %s", RES_OK, pThisSession->sid);
	pThisSession->pAcc = NULL;
	printf("Log out successful.\n");
	return;
}

/*
Process reauth request and encapsulate response
*/
void processReauth(char* buff, char* sid) {
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
		snprintf(buff, BUFF_LEN, "%d", RES_RA_SID_NOT_FOUND);
		return;	
	}
	// Check if there's an account attached to session
	if (pThisSession->pAcc == NULL) {
		printf("Session exists but not logged in..\n");
		snprintf(buff, BUFF_LEN, "%d", RES_RA_FOUND_NOTLI);
		return;
	}
	// Check if lastActive is more than 1 day
	if (now - pThisSession->lastActive > TIME_1_DAY) {
		printf("Login session timeout. Deny reauth.\n");
		pThisSession->pAcc = NULL;
		snprintf(buff, BUFF_LEN, "%d", RES_RA_FOUND_NOTLI);
		return;
	}
	// Check if account is disabled
	if (pThisSession->pAcc->isLocked) {
		printf("Account is locked. Reauth failed.\n");
		pThisSession->pAcc = NULL;
		snprintf(buff, BUFF_LEN, "%d", RES_LI_ACC_LOCKED);
		return;
	}
	// Check if account is logged in on another device
	WaitForSingleObject(sessionListMutex, INFINITE);
	for (it = sessionList.begin(); it != sessionList.end(); it++) {
		if (((Session*)&(*it) != pThisSession)
			&& (it->pAcc == pThisSession->pAcc)
			&& (now - it->lastActive < TIME_1_DAY))
		{
			printf("This account is logged in on another client.\n");
			pThisSession->pAcc = NULL;
			snprintf(buff, BUFF_LEN, "%d", RES_LI_ELSEWHERE);
			ReleaseMutex(sessionListMutex);
			return;
		}
	}
	// All checks out!
	printf("Session exists. Allow reauth.\n");
	pThisSession->lastActive = time(0);
	snprintf(buff, BUFF_LEN, "%d %s", RES_OK, pThisSession->sid);
	ReleaseMutex(sessionListMutex);
	return;
}

/*
Process sid request and encapsulate response
*/
void processRequestSid(char* buff, char* sid) {
	WaitForSingleObject(sessionListMutex, INFINITE);

	genSid(sid);

	// Create new session and add to sessionList
	Session newSession;
	newSession.pAcc = NULL;
	newSession.lastActive = 0;
	strcpy_s(newSession.sid, SID_LEN, sid);
	sessionList.push_back(newSession);

	snprintf(buff, BUFF_LEN, "%d %s", RES_OK, newSession.sid);
	ReleaseMutex(sessionListMutex);
	printf("Generated new SID for client: %s.\n", sid);
}

/*
Extract Sid from req and the provided index
return 0 if Sid is valid, else return 1
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
*/
void parseRequest(char* buff) {
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
		processLogIn(buff, sid, username, password);
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
		processReauth(buff, sid);
	}
	else if (_stricmp(command, "REQUESTSID") == 0) {
		// Process Sid request
		processRequestSid(buff, sid);
	}
	else {
		processCorruptedMsg(buff);
	}
}

unsigned __stdcall procNewClient(void *sock) {
	SockInfo* newSock = (SockInfo*)sock;
	char buff[BUFF_LEN];
	int iRetVal;

	while (1) {
		iRetVal = recvReq(newSock, buff, BUFF_LEN);
		if (iRetVal == 1) break;

		parseRequest(buff);

		iRetVal = sendRes(newSock, buff, strlen(buff));
		if (iRetVal == 1) break;
	}
	closesocket(newSock->connSock);
	free(sock);
	return 0;
}

/*
Application driver
*/
void run() {
	// Initialize Mutex
	sessionListMutex = CreateMutex(NULL, false, NULL);

	while (1) {
		// Initialize a new SockInfo
		SockInfo* newSock = (SockInfo*)malloc(sizeof(SockInfo));
		newSock->clientAddrLen = sizeof(newSock->clientAddr);

		// Accept connection
		int iRetVal = acceptConn(newSock);
		if (iRetVal == 1)
			break;
		_beginthreadex(0, 0, procNewClient, (void*)newSock, 0, 0);
	}
	CloseHandle(sessionListMutex);
}