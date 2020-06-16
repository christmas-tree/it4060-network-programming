#pragma once

#ifndef IO_UTILS_
#define IO_UTILS_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <list>

#ifndef FD_SETSIZE
#define FD_SETSIZE 3
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
	SOCKET		socket;
} Session;

typedef struct {
	Account*	pAcc;
	int			numOfAttempts;
	time_t		lastAtempt;
} Attempt;

/*
Initialize network environment. Return 0 if initialize successful, else return 1
[IN] serverPortNumber:	a short containing server's port number to be initialized
*/
int initConnection(short serverPortNumber);

/*
Cleanup Winsock instance and free resources
*/
void ioCleanup();

/*
Accept connection. Return 1 if fail to accept, return 0 if succeed
[OUT] sock:		a variable to store value of the socket assigned
				to the accepted connection
*/
int acceptConn(SOCKET* sock);

/*
Receive request from specified socket. Return 1 if fail to receive, return 0 if succeed
[IN] sock:		a socket from which the request is received
[OUT] req:		a char array to store received request
[IN] reqLen:	an int containing the maximum length of char array req
*/
int recvReq(SOCKET sock, char* req, int reqLen);

/*
Send response to specified socket, return 1 if fail to send, return 0 if succeed
[IN] sock:		a socket to which the response is sent
[IN] res:		a char array containing the response to be sent
[IN] resultLen:	an int containing the length of response
*/
int sendRes(SOCKET sock, char* res, int resultLen);

/*
Open file and store file name in a global variable. If file does not exists,
create new file. Return 0 if succeed, else return an error code.
[IN] fileName:	a char array containing the name of the file to be opened
*/
int openFile(char* fileName);

/*
Read account data from opened file, initialize data structure
and add to accList. Return 1 if fail, return 0 if succeed
[OUT] accList:	a vector to store the instances of Accounts
				read from file
*/
int readUserF(std::list<Account> &accList);

/*
Stringify account data and write to file. Return 1 if fail, return 0 if succeed
[IN] accList:	a vector containing instances of Account to be written to file
*/
int writeUserF(std::list<Account> accList);

#endif