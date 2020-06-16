#pragma once

#ifndef IO_UTILS_H_
#define IO_UTILS_H_

#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

int initConnection(char* serverIpAddr, short serverPortNumbr);
void ioCleanup();
int sendReq(char* buff, int bufflen);

int openFile(char* fileName);
int readSidFromF(char* sid, int sidLen);
int writeSidToF(char* sid);
#endif