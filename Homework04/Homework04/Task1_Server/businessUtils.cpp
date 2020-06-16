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
#define RES_LO_NOT_LI			411
#define RES_RA_FOUND_NOTLI		420
#define RES_RA_SID_NOT_FOUND	424

#define TIME_1_DAY				86400
#define TIME_1_HOUR				3600
#define ATTEMPT_LIMIT			3

#define MAX_LEN 1024

#define LIREQ_MINLEN			5 + SID_LEN + 1
#define LOREQ_MINLEN			6 + SID_LEN
#define RAREQ_MINLEN			6 + SID_LEN

std::list<Attempt> attemptList;
std::list<Session> sessionList;
std::list<Account> accountList;
char req[MAX_LEN];
char res[MAX_LEN];

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
	srand(time(NULL));
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
Process login from sid, username and password
and encapsulate response
*/
void processLogIn(char* sid, char* username, char* password) {
	Account* pAccount;
	Session* pThisSession;
	time_t now = time(0);

	// Check exists
	pAccount = findAccount(username);
	if (pAccount == NULL) {
		printf("Account not found!\n");
		snprintf(res, MAX_LEN, "%d", RES_LI_ACC_NOT_FOUND);
		return;
	}

	// Check if account is locked
	if (pAccount->isLocked) {
		printf("Account is locked.\n");
		snprintf(res, MAX_LEN, "%d", RES_LI_ACC_LOCKED);
		return;
	}

	// Check password
	if (strcmp(pAccount->password, password) != 0) {
		printf("Wrong password!\n");

		// Check if has attempt before
		for (std::list<Attempt>::iterator it = attemptList.begin(); it != attemptList.end(); it++) {
			if ((*it).pAcc == pAccount) {

				// If last attempt is more than 1 hour before then reset number of attempts
				if ((*it).lastAtempt - now > TIME_1_HOUR)
					(*it).numOfAttempts = 1;
				else
					(*it).numOfAttempts++;
				(*it).lastAtempt = now;

				// If number of attempts exceeds limit then block account and update file 
				if ((*it).numOfAttempts > ATTEMPT_LIMIT) {
					pAccount->isLocked = true;
					writeUserF(accountList);
					printf("Account locked. File updated.\n");
					snprintf(res, MAX_LEN, "%d", RES_LI_ACC_LOCKED);
					return;
				}
				snprintf(res, MAX_LEN, "%d", RES_LI_WRONG_PASS);
				return;
			}
		}
		// if there is no previous attempt, add attempt
		Attempt newAttempt;
		newAttempt.lastAtempt = now;
		newAttempt.numOfAttempts = 1;
		newAttempt.pAcc = pAccount;
		attemptList.push_back(newAttempt);

		snprintf(res, MAX_LEN, "%d", RES_LI_WRONG_PASS);
		return;
	}

	// Check if user has already logged in
	for (std::list<Session>::iterator it = sessionList.begin(); it != sessionList.end(); it++) {
		if (strcmp(sid, it->sid) == 0) {
			pThisSession = (Session*)&(*it);
			// If already logged in on this window then deny log in
			if (pThisSession->pAcc != NULL) {
				printf("Account is already logged in on this client.\n");
				snprintf(res, MAX_LEN, "%d", RES_LI_ALREADY_LI);
				return;
			}
			// If haven't logged in, update active time and session account info
			else {
				pThisSession->pAcc = pAccount;
				pThisSession->lastActive = now;
				printf("Login successful.\n");
				snprintf(res, MAX_LEN, "%d %s", RES_OK, sid);
				return;
			}
		}
	}
}

/*
Process log out and encapsulate response
*/
void processLogOut(char* sid) {
	// Check if user is logged in
	std::list<Session>::iterator it;
	// Find session
	for (it = sessionList.begin(); it != sessionList.end(); it++) {
		if (strcmp(sid, it->sid) == 0) {
			// If there's an account attached to session => allow log out
			if (it->pAcc != NULL) {
				printf("Account is logged in. Log out OK.\n");
				snprintf(res, MAX_LEN, "%d %s", RES_OK, it->sid);
				sessionList.erase(it);
				printf("Log out successful.\n");
				return;
			}
			break;
		}
	}
	// Deny log out
	printf("Account is not logged in.\n");
	snprintf(res, MAX_LEN, "%d", RES_LO_NOT_LI);
	return;
}

/*
Process reath request and encapsulate response
*/
void processReauth(char* sid) {
	time_t now = time(0);

	// Check if session exists
	std::list<Session>::iterator it;
	for (it = sessionList.begin(); it != sessionList.end(); it++) {
		if (strcmp(sid, it->sid) == 0) {

			// Check if there's an account attached to session
			if (it->pAcc != NULL) {
				// Check if lastActive is less than 1 day
				if (it->lastActive - now < TIME_1_DAY) {
					printf("Session exists. Allow reauth.\n");
					it->lastActive = time(0);
					snprintf(res, MAX_LEN, "%d %s", RES_OK, it->sid);
					return;
				}
				// Session timeout
				else {
					printf("Login session timeout. Deny reauth.\n");
					snprintf(res, MAX_LEN, "%d", RES_RA_FOUND_NOTLI);
					return;
				}
			}
			// Session is not attached to any account
			else {
				printf("Session exists but not logged in..\n");
				snprintf(res, MAX_LEN, "%d", RES_RA_FOUND_NOTLI);
				return;
			}
		}
	}
	// Cannot find existing session with this sid
	printf("Session not found.\n");
	snprintf(res, MAX_LEN, "%d", RES_RA_SID_NOT_FOUND);
	return;
}

/*
Process sid request and encapsulate response
*/
void processRequestSid(char* sid) {
	genSid(sid);

	// Create new session and add to sessionList
	Session newSession;
	newSession.pAcc = NULL;
	newSession.lastActive = 0;
	strcpy_s(newSession.sid, SID_LEN, sid);
	sessionList.push_back(newSession);

	snprintf(res, MAX_LEN, "%d %s", RES_OK, newSession.sid);
	printf("Generated new SID for client: %s.\n", sid);
}

/*
Process corrupted message
*/
void processCorruptedMsg() {
	printf("Cannot read request. Request corrupted.\n");
	snprintf(res, MAX_LEN, "%d", RES_BAD_REQUEST);
	return;
}

/*
Extract Sid from req and the provided index
return 0 if Sid is valid, else return 1
*/
int extractSid(char* sid, int &i) {
	int j = 0;
	i++;
	while (j < SID_LEN - 1) {
		if (req[i] == ' ') {
			sid[0] = 0;
			return 1;
		}
		sid[j++] = req[i++];
	}
	sid[j] = 0;
	return 0;
}

/*
Extract command from request and call corresponding process function
*/
void parseRequest() {
	char command[20];
	char sid[SID_LEN];

	// Extract command
	int i, j;
	for (i = 0; req[i] != ' ' && req[i] != 0; i++) {
		command[i] = req[i];
	}
	command[i] = 0;
	
	// Execute command
	if (_stricmp(command, "LOGIN") == 0) {
		// Validate request and extract SID
		if (strlen(req) < LIREQ_MINLEN) {
			processCorruptedMsg();
			return;
		}
		if (extractSid(sid, i)) {
			processCorruptedMsg();
			return;
		}
		// Extract username and password
		char username[CRE_MAXLEN];
		char password[CRE_MAXLEN];

		j = 0;
		for (i = i + 1; req[i] != ' ' && req[i] != 0; i++) {
			username[j++] = req[i];
		}
		username[j] = 0;

		j = 0;
		for (i = i + 1; req[i] != ' ' && req[i] != 0; i++) {
			password[j++] = req[i];
		}
		password[j] = 0;

		// Process Login
		processLogIn(sid, username, password);
	}
	else if (_stricmp(command, "LOGOUT") == 0) {
		// Validate request and extract SID
		if (strlen(req) < LOREQ_MINLEN) {
			processCorruptedMsg();
			return;
		}
		if (extractSid(sid, i)) {
			processCorruptedMsg();
			return;
		}
		// Process Login
		processLogOut(sid);
	}
	else if (_stricmp(command, "REAUTH") == 0) {
		// Validate request and extract SID
		if (strlen(req) < RAREQ_MINLEN) {
			processCorruptedMsg();
			return;
		}
		if (extractSid(sid, i)) {
			processCorruptedMsg();
			return;
		}
		// Process Login
		processReauth(sid);
	}
	else if (_stricmp(command, "REQUESTSID") == 0) {
		// Process Sid request
		processRequestSid(sid);
	}
	else {
		processCorruptedMsg();
	}
}

int start() {
	while (1) {
		int iRetVal = acceptReq(req, MAX_LEN);
		if (iRetVal == 0) {
			parseRequest();
			sendRes(res, strlen(res));
		}
	}
}