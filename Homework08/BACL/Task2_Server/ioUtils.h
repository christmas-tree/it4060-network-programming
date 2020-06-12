#pragma once
#ifndef IO_UTILS_
#define IO_UTILS_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <time.h>

#define RECVMSG_ERROR -2
#define FILENAME_LEN 16

#define FILEBUFF_SIZE 1024*16+64
#define BUFF_SIZE 1024
#define PART_SIZE 1024*16

#define OP_ENC			0
#define OP_DEC			1
#define OP_FILE			2
#define OP_ERR			3

int initConnection(short serverPortNumbr);
void networkCleanup();

int acceptConn(SOCKET* sock);
int recvPacket(SOCKET sock, short* opCode, short* len, char* payload, short payloadMaxLen);
int sendPacket(SOCKET sock, short opCode, short len, char* payload);
int recvFile(SOCKET sock, char* fileName);
int sendFile(SOCKET sock, char* fileName);

#endif