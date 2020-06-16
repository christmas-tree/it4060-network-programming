#ifndef BUSINESSUTILS_H_
#define BUSINESSUTILS_H_

#include <stdlib.h>
#include <conio.h>

#include "ioUtils.h"

#define BUFF_SIZE 1024

/*
Prepare data. Return 0 if successful, return value > 0 if fail
*/
int initData();

/*
Application driver
*/
void run();

#endif