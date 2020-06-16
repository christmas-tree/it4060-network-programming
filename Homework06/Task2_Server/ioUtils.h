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

#define MAX_THREADCOUNT 100
#define RECVMSG_CORRUPT 2

#define DNS_RESOLVE_FAILURE -1
#define DNS_RESOLVE_SUCCESS 1

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

#endif