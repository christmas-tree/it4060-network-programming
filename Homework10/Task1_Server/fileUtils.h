#pragma once

#ifndef IO_UTILS_
#define IO_UTILS_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <list>
#include <winsock2.h>

#define CRE_MAXLEN		31
#define SID_LEN			17

#define BUFSIZE			8192
#define	READ			0
#define WRITE			1

// Structure definition
typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuff;
	CHAR buffer[BUFSIZE];
	int bufLen;
	int recvBytes;
	int sentBytes;
	int intendedLength;
	int operation;
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET socket;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
	char username[CRE_MAXLEN];
	char password[CRE_MAXLEN];
	bool isLocked;
} Account;

typedef struct {
	char		sid[SID_LEN];
	Account*	pAcc;
	time_t		lastActive;
	SOCKET		socket;
} Session;

typedef struct {
	Account*	pAcc;
	int			numOfAttempts;
	time_t		lastAtempt;
} Attempt;

// Shared variables
FILE* f;
char* fName;
char filebuff[BUFSIZE];

/*
Open file and store file name in a global variable. If file does not exists,
create new file. Return 0 if succeed, else return an error code.
[IN] fileName:	a char array containing the name of the file to be opened
*/
int openFile(char* fileName) {
	int retVal = fopen_s(&f, fileName, "r+");
	fName = fileName;
	if (f == NULL) {
		printf("File does not exist. Creating new file.\n");
		retVal = fopen_s(&f, fileName, "w+");
		if (retVal)
			printf("Cannot create new file.\n");
	}
	printf("Open file successfully.\n");
	return retVal;
}

/*
Read account data from opened file, initialize data structure
and add to accList. Return 1 if fail, return 0 if succeed
[OUT] accList:	a vector to store the instances of Accounts
read from file
*/
int readUserF(std::list<Account>& accList) {
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	// Read file
	while (!feof(f)) {
		Account acc;
		fgets(filebuff, BUFSIZE, f);

		if (strlen(filebuff) == 0) continue;
		int i = 0;
		int j = 0;
		//Split user name
		while (filebuff[i] != ' ') {
			acc.username[j] = filebuff[i];
			i++;
			j++;
		}
		acc.username[j] = '\0';
		j = 0;
		i++;
		//Split password
		while (filebuff[i] != ' ') {
			acc.password[j] = filebuff[i];
			i++;
			j++;
		}
		acc.password[j] = '\0';
		i++;
		// Split status
		acc.isLocked = (filebuff[i] == '1');
		accList.push_back(acc);
	}
	printf("Data loaded.\n");
	return 0;
}

/*
Stringify account data and write to file. Return 1 if fail, return 0 if succeed
[IN] accList:	a vector containing instances of Account to be written to file
*/
int writeUserF(std::list<Account> accList) {
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_SET);
	filebuff[0] = '\0';

	// Write the first n-1 lines
	std::list<Account>::iterator it = accList.begin();
	std::list<Account>::iterator endIt = accList.end();
	endIt--;
	for (; it != endIt; it++) {
		snprintf(filebuff, BUFSIZE, "%s %s %d\n",
			(*it).username, (*it).password, (*it).isLocked);
		fputs(filebuff, f);
	}
	// Write last line
	snprintf(filebuff, BUFSIZE, "%s %s %d",
		(*it).username, (*it).password, (*it).isLocked);
	fputs(filebuff, f);
	// Close file
	fclose(f);
	openFile(fName);

	return 0;
}

#endif