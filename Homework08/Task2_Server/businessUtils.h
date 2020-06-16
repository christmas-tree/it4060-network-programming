#ifndef BUSINESSUTILS_H_
#define BUSINESSUTILS_H_

#include <stdlib.h>
#include <conio.h>
#include <time.h>

#include "md5.h"
#include "ioUtils.h"


typedef struct Client {
	SOCKET		sock = 0;
	char		fileName[FILENAME_LEN + 1];
	char		fileNameNew[FILENAME_LEN + 5];
	char		fileNewDigest[DIGEST_SIZE];
	FILE*		file = NULL;
	short		key = 0;
	short		operation = -1;
	bool		isTransfering = false;
	bool		isCompleted = true;
	short		filePart = 0;
} Client;


void run();

#endif