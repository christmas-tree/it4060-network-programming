#pragma once

#ifndef IO_UTILS_
#define IO_UTILS_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <list>

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#define CRE_MAXLEN 31
#define SID_LEN 17
#define MAX_THREADCOUNT 100

#define BUFF_SIZE 100
#define RECVMSG_CORRUPT 2

typedef struct {
	char username[CRE_MAXLEN];
	char password[CRE_MAXLEN];
	bool isLocked;
} Account;

typedef struct {
	char		sid[SID_LEN];
	Account*	pAcc;
	time_t		lastActive;
} Session;

typedef struct {
	Account*	pAcc;
	int			numOfAttempts;
	time_t		lastAtempt;
} Attempt;

/*
Initialize network environment, return 0
if initialize successful else return 1
*/
int initConnection(short serverPortNumbr);
void ioCleanup();

/*
Accept connection. Return 1 if fail to accept, return 0 if succeed
*/
int acceptConn(SOCKET* sock);

/*
Receive request from specified socket. Return 1 if fail to receive, return 0 if succeed
*/
int recvReq(SOCKET sock, char* req, int reqLen);

/*
Send response to clientList, return 1 if fail to send, return 0 if succeed
*/
int sendRes(SOCKET sock, char* res, int resultLen);

/*
Open file and store file name in fname. If file does not exists,
create new file. Return 0 if succeed, return an error code if fail.
*/
int openFile(char* fileName);

/*
Read account data from opened file, init data structure
and add to accList. Return 1 if fail, return 0 if succeed
*/
int readUserF(std::list<Account> &accList);

/*
Stringify account data and write to file. Return 1 if fail, return 0 if succeed
*/
int writeUserF(std::list<Account> accList);

#endif