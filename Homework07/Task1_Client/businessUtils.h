#pragma once

#ifndef BUSINESS_UTILS_H_
#define BUSINESS_UTILS_H_

#include "ioUtils.h"

#include <stdlib.h>
#include <conio.h>

#define BUFF_LEN 1024
#define SID_LEN 17

#define RES_OK					200
#define RES_BAD_REQUEST			400
#define RES_LI_WRONG_PASS		401
#define RES_LI_ALREADY_LI		402
#define RES_LI_ACC_LOCKED		403
#define RES_LI_ACC_NOT_FOUND	404
#define RES_LI_ELSEWHERE		405
#define RES_LO_NOT_LI			411
#define RES_RA_FOUND_NOTLI		420
#define RES_RA_SID_NOT_FOUND	424

/*
Send Reauthenticate request to server and process response if a SID can be found.
return -1 if sid data cannot be read from file, return 1 if cannot send to or recv
from server, return status code from server if received
*/
int reqReauth();

/*
Send log out request to server and process response, return 1 if fail to send or recv
else return status code from server
*/
int reqLogOut();

/*
Send log in request to server and process response, return 1 if fail to send or recv
else return status code from server
*/
int reqLogIn(char* username, char* password);

/*
Send SID Request to server and process response, return 1 if fail to send or recv
return status code from server if received
*/
int reqSid();

#endif // !BUSINESS_UTILS_H_