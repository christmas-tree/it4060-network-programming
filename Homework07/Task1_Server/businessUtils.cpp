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
	
std::list<Attempt> attemptList;
std::list<Session> sessionList;
std::list<Account> accountList;

int threadCount = 0;
HANDLE sessionListMutex;
HANDLE threadStatus[MAX_THREADCOUNT];

extern SOCKET listenSock;

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
			printf("This account is logged in on another clientList.\n");
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
			printf("This account is logged in on another clientList.\n");
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
	printf("Generated new SID for clientList: %s.\n", sid);
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

/*
Thread's main function. Accept new connection and scan sockets
for activities. Call corresponding functions to process message
and send response to client.
*/
unsigned __stdcall procThread(void *threadNumber) {
	// Initialize Variables
	int threadId = *((int*)threadNumber) - 1;
	char buff[BUFF_LEN];
	SOCKET clientList[FD_SETSIZE] = {0};
	SOCKET connSock;
	fd_set initfds, readfds;
	int iRetVal, nEvents, clientCount = 0;

	// Get control of thread mutex
	WaitForSingleObject(threadStatus[threadId], INFINITE);
	printf("New thread ID %d created %d.\n", threadId, GetCurrentThreadId());
	
	FD_ZERO(&initfds);
	FD_SET(listenSock, &initfds);

	while (1) {
		readfds = initfds;
		nEvents = select(0, &readfds, 0, 0, 0);

		if (nEvents < 0) {
			printf("Error %d! Cannot poll sockets.\n", WSAGetLastError());
			break;
		}

		// Check listenSock and accept if thread's capacity is not full
		if (FD_ISSET(listenSock, &readfds)
			&& clientCount < FD_SETSIZE - 1
			&& acceptConn(&connSock) == 0)
		{
			// Store new socket and add socket to initfds
			int i;
			for (i = 0; i < FD_SETSIZE; i++)
				if (clientList[i] == 0) {
					clientList[i] = connSock;
					FD_SET(clientList[i], &initfds);
					// if thread's capacity is full, release thread's mutex
					if (++clientCount == FD_SETSIZE - 1)
						ReleaseMutex(threadStatus[threadId]);
					break;
				}
			if (--nEvents == 0)
				continue;
		}

		// Check for activities in other sockets
		for (int i = 0; i < FD_SETSIZE; i++) {
			if (clientList[i] == 0)
				continue;

			if (FD_ISSET(clientList[i], &readfds)) {
				iRetVal = recvReq(clientList[i], buff, BUFF_LEN);
				if (iRetVal == 0) {
					parseRequest(buff);
					iRetVal = sendRes(clientList[i], buff, strlen(buff));
				}
				if (iRetVal == 1) {
					// Cleanup socket
					FD_CLR(clientList[i], &initfds);
					closesocket(clientList[i]);
					clientList[i] = 0;
					// if thread's capacity is not full, get control of thread's mutex
					if (--clientCount == FD_SETSIZE - 2)
						WaitForSingleObject(threadStatus[threadId], INFINITE);
				}
				if (--nEvents == 0)
					break;
			}
		}
	}
	return 0;
}

void run() {
	// Initialize Mutex
	sessionListMutex = CreateMutex(NULL, false, NULL);

	// Start new thread and create thread's mutex
	threadCount++;
	_beginthreadex(0, 0, procThread, (void*)&threadCount, 0, 0);
	threadStatus[threadCount-1] = CreateMutex(NULL, false, NULL);

	while (1) {
		// main thread sleeps so newly created thread can set up
		Sleep(500);

		// Wait until the mutexes of all threads are released, i.e all threads are full
		// to create new thread
		WaitForMultipleObjects(threadCount, threadStatus, true, INFINITE);
		if (threadCount <= MAX_THREADCOUNT) {
			threadCount++;
			_beginthreadex(0, 0, procThread, (void*)&threadCount, 0, 0);
			threadStatus[threadCount - 1] = CreateMutex(NULL, false, NULL);

			// Release threads' mutexes
			for (int i = 0; i < threadCount; i++) {
				ReleaseMutex(threadStatus[i]);
			}
		}
		else {
			printf("Maximum capacity reached.\n");
		}
	}

	// Cleanup Handles
	CloseHandle(sessionListMutex);
	for (int i = 0; i < threadCount; i++)
		CloseHandle(threadStatus[i]);
}