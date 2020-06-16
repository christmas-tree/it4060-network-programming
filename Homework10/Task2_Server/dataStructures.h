#pragma once

#ifndef DATA_STRUCTURES_H_
#define DATA_STRUCTURES_H_

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <winsock2.h>
#include <process.h>
#include <list>
#include <queue>

#include "md5.h"
#pragma comment(lib, "Ws2_32.lib")

#define RECEIVE			0
#define SEND			1
#define MAX_CLIENT		10000

#define FILENAME_SIZE	150
#define DIGEST_SIZE		33
#define BUFSIZE			8192
#define PAYLOAD_SIZE	8150

// Structure definition

typedef struct {
	short opCode;
	int payloadLength;
	int offset;
	char payload[PAYLOAD_SIZE];
} MESSAGE, *LPMESSAGE;

typedef struct {
	char		fileName[FILENAME_SIZE];
	char		digest[DIGEST_SIZE];
	FILE*		file = NULL;
	bool		isTransfering = false;
	short		filePart = 0;
}FILE_TRANSFER_PROPERTY, *LPFILE_TRANSFER_PROPERTY;

typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF		dataBuff;
	char		buffer[BUFSIZE];
	int			recvBytes;
	int			sentBytes;
	int			operation;
	LPFILE_TRANSFER_PROPERTY fileProp;
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET		socket;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
	SOCKET		socket;
	char		buff[BUFSIZE];
	LPPER_IO_OPERATION_DATA perIoData;
} PER_QUEUE_ITEM, *LPPER_QUEUE_ITEM;

#endif