
#include "stdafx.h"
#include "uiUtils.h"

char buff[BUFF_SIZE];

/*
Get input from stdin. If input is longer than buffer len
then flush the input buffer. Remove the newline character
at string end if exists. Return 1 if input is empty, else
return 0.
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

void showResponse(char* message) {
	if (message[0] == DNS_RESOLVE_FAILURE) {
		printf("Not found information");
	}
	else if (message[0] == DNS_RESOLVE_SUCCESS) {
		printf("%s", message + 1); // Print string except the first byte.
	}
	else {
		printf("Message from server corrupted!");
	}
	printf("\n");
}

void startUI() {
	bool cont = true;
	// Loop until user enters an empty string.
	while (cont) {
		// Get input
		printf("\nEnter IP Address or Hostname (maximum %d characters):\n", BUFF_SIZE - 1);
		printf("=======> ");
		while (1) {
			// Get file name. If user enters empty string, break loop.
			if (getInput(buff, BUFF_SIZE - 1)) {
				cont = false;
				break;
			}
			// Validate input
			char* pDotChar = strrchr(buff, '.');
			char* pSpaceChar = strrchr(buff, ' ');
			if (pDotChar != NULL && strlen(pDotChar) > 0 && pSpaceChar == NULL)
				break;
			printf("\nInvalid input. Please try again (maximum %d characters):\n", BUFF_SIZE - 1);
			printf("=======> ");
		}
		if (!cont)
			break;
		if (sendAndRecv(buff, BUFF_SIZE) == 1) break;
		showResponse(buff);
	}
}