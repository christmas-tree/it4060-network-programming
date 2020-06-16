#pragma once

#ifndef UI_UTILS_H_
#define UI_UTILS_H_

#include "businessUtils.h"
#include "uiUtils.h"

/*
Function gets input from stdin. If input is longer than buffer len
then flush the input buffer. Remove the newline character
at string end if exists. Return 1 if input is empty, else
return 0.
[OUT] s:		a char array to store the input
[IN] maxLen:	the maximum length of the char array
*/
bool getInput(char* s, size_t maxLen) {
	fgets(s, maxLen, stdin);
	int slen = strlen(s);
	// If s is an emptry string, return 1
	if ((slen == 0) || ((slen == 1) && (s[0] = '\n')))
		return 1;
	// Purge newline character if exists
	if (s[slen - 1] == '\n')
		s[slen - 1] = 0;
	else {
		// Flush stdin
		char ch;
		while ((ch = fgetc(stdin)) != '\n' && ch != EOF);
	}
	return 0;
}

/*
Show the user interface, ask for user input and evaluate user input.
IF user input is correct, call corresponding process functions.
*/
void startUI() {
	FILE* f;
	bool cont = true;

	// Loop until user enters an empty string.
	while (cont) {
		char filePath[FILENAME_SIZE];

		printf("\nEnter file path to upload to server (maximum length allowed: %d characters):\n", FILENAME_SIZE - 4);
		printf("=======> ");
		while (1) {
			// Get file name. If user enters empty string, break loop.
			if (getInput(filePath, FILENAME_SIZE - 4)) {
				cont = false;
				break;
			}
			// Check if file exists
			f = fopen(filePath, "rb");
			if (f == NULL) {
				printf("Cannot open specified file. Please enter file path again:\n");
				printf("=======> ");
			}
			else {
				fclose(f);
				break;
			}
		}
		if (!cont) break;

		sendFile(filePath);
	}
}

#endif

