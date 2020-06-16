#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "md5.h"

#define OP_ENC			0
#define OP_DEC			1
#define OP_FILE			2
#define OP_ERR			3

#define RECVMSG_ERROR -2
#define FILEBUFF_SIZE 1024*16+64
#define PART_SIZE 1024*16
#define BUFF_SIZE 1024
#define DIGEST_SIZE 33

#define FILENAME_SIZE 1024
#define KEY_SIZE 4

int initConnection(char* serverIpAddr, short serverPortNumbr);
void networkCleanup();

int recvPacket(short* opCode, short* len, char* payload, short payloadMaxLen);
int sendPacket(short opCode, short len, char* payload);
int recvFile(char* fileName);
int sendFile(char* fileName);
#endif