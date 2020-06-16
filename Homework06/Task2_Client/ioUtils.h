#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#define RECVMSG_CORRUPT 2
#define DNS_RESOLVE_FAILURE -1
#define DNS_RESOLVE_SUCCESS 1

/*
Initialize network environment, return 0 if initialize successful else return 1
*/
int initConnection(char* serverIpAddr, short serverPortNumbr);

/*
Close socket and cleanup Winsock
*/
void ioCleanup();

/*
Send request in buff to server, receive message and fill in buff.
Return 1 if fail, return 0 if succeed
*/
int sendAndRecv(char* buff, int bufflen);

#endif