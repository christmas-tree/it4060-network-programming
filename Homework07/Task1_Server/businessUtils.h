#ifndef BUSINESSUTILS_H_
#define BUSINESSUTILS_H_

#include <stdlib.h>
#include <conio.h>
#include <time.h>

#include "ioUtils.h"

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

#define TIME_1_DAY				86400
#define TIME_1_HOUR				3600
#define ATTEMPT_LIMIT			3

#define BUFF_LEN 1024

#define LIREQ_MINLEN			5 + SID_LEN + 1
#define LOREQ_MINLEN			6 + SID_LEN
#define RAREQ_MINLEN			6 + SID_LEN

/*
Prepare data. Return 0 if successful, return value > 0 if fail
*/
int initData();

/*
Application driver
*/
void run();

#endif