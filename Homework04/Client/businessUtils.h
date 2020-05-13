#pragma once

#ifndef BUSINESS_UTILS_H_
#define BUSINESS_UTILS_H_

#include "ioUtils.h"

#include <stdlib.h>
#include <conio.h>

#define RES_OK					200
#define RES_BAD_REQUEST			400
#define RES_LI_WRONG_PASS		401
#define RES_LI_ALREADY_LI		402
#define RES_LI_ACC_LOCKED		403
#define RES_LI_ACC_NOT_FOUND	404
#define RES_LO_NOT_LI			411
#define RES_RA_FOUND_NOTLI		420
#define RES_RA_SID_NOT_FOUND	424

int reqReauth();
int reqLogOut();
int reqLogIn(char* username, char* password);
int reqSid();

#endif // !BUSINESS_UTILS_H_