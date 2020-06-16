#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#define RECVMSG_CORRUPT 2

/*
Initialize network environment. Return 0 if initialize successful, else return 1
[IN] serverIpAddr:		a char array containing server's address to be initialized
[IN] serverPortNumber:	a short containing server's port number to be initialized
*/
int initConnection(char* serverIpAddr, short serverPortNumbr);

/*
Cleanup Winsock instance and free resources
*/
void ioCleanup();

/*
Send request to server and receive response. Return 1 if fail, return 0 if succeed
[IN/OUT] buff:	a char array containing request and stores received response
[IN] bufflen:	an int containing the maximum length of buff
*/
int sendReq(char* buff, int bufflen);

/*
Open file and store file name in a global variable. If file does not exists,
create new file. Return 0 if succeed, else return an error code.
[IN] fileName:	a char array containing the name of the file to be opened
*/
int openFile(char* fileName);

/*
Read SID from opened file. Return 1 if fail, return 0 if succeed.
[OUT] sid:		a char array to store sid read from file
[IN] sidLen:	the length of a SID
*/
int readSidFromF(char* sid, int sidLen);
/*
Write sid to opened file. Return 1 if fail, return 0 if succeed.
[IN] sid:	a char array containing SID to be written to file
*/
int writeSidToF(char* sid);
#endif