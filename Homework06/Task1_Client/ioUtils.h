#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#define RECVMSG_CORRUPT 2

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
int sendReq(char* buff, int bufflen);

/*
Open file and store file name. If file does not exists, create new file.
Return 0 if succeed, return an error code if fail.
*/
int openFile(char* fileName);

/*
Read SID from opened file. Return 1 if fail, return 0 if succeed.
*/
int readSidFromF(char* sid, int sidLen);
/*
Write sid to opened file. Return 1 if fail, return 0 if succeed.
*/
int writeSidToF(char* sid);
#endif