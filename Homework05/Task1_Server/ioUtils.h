#pragma once
#ifndef IO_UTILS_
#define IO_UTILS_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <list>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#define CRE_MAXLEN 31
#define SID_LEN 17
#define CLIENT_ADDR_LEN 25

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

typedef struct {
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen;
} SockInfo;

int initConnection(short serverPortNumbr);
void ioCleanup();

int acceptConn(SockInfo* sock);
int recvReq(SockInfo* sock, char* req, int reqLen);
int sendRes(SockInfo* sock, char* res, int resultLen);

int openFile(char* fileName);
int readUserF(std::list<Account> &accList);
int writeUserF(std::list<Account> accList);

#endif