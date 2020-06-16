
#include "stdafx.h"
#include "uiUtils.h"

char filePath[FILENAME_SIZE];
char key[KEY_SIZE];
FILE* f;

/*
Check if a string is a number
*/
bool isNumber(char* s) {
	for (int i = 0; s[i] != '\0'; i++) {
		if (!isdigit(s[i]))
			return false;
	}
	return true;
}

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
	if ((slen == 0) || ((slen == 1) && (s[0]='\n')))
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
Start user interface
*/
void startUI() {
	char ch;
	bool cont = true;
	bool op;
	// Loop until user enters an empty string.
	while (cont) {
		printf("\nEnter file path (maximum length allowed: %d characters):\n", FILENAME_SIZE-4);
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

		// Get operation
		printf("Encode or Decode (e/d): ");
		while (1) {
			ch = _getch();
			printf("%c\n", ch);
			if (ch == 'e' || ch == 'E') {
				op = OP_ENC;
				break;
			}
			else if (ch == 'd' || ch == 'D') {
				op = OP_DEC;
				break;
			}
			else
				printf("Wrong operation choice. Please choose again: (e/d)\n");
		}
		// Get key
		printf("Enter key: (> 0 and < 999) ");
		while (1) {
			// If input string is blank or is not a number, force reinput
			if (getInput(key, KEY_SIZE) || !isNumber(key))
				printf("\nNot an integer. Please enter key again: ");
			else
				break;
		}

		// Process request
		if (procReq(op, filePath, key)) {
			printf("Operation failed!\n");
			cont = false;
		}
		else {
			printf("Operation succeeded!\n%s file saved at %s.\n", (op == OP_ENC) ? "Encoded" : "Decoded", filePath);
		}
	}
}

