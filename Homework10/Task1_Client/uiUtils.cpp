
#include "stdafx.h"
#include "uiUtils.h"

#define CRE_MAXLEN 31

bool isLoggedIn = false;
bool quit = false;
char username[CRE_MAXLEN] = "";
char password[CRE_MAXLEN] = "";

/*
Read password input, show asterisks instead of password characters to screen
[OUT] password:		a char array to store password that user entered
*/
void getPassword(char *password) {
	char ch = 0;
	int i = 0;

	// Get console mode and disable character echo
	DWORD conMode, dwRead;
	HANDLE handleIn = GetStdHandle(STD_INPUT_HANDLE);

	GetConsoleMode(handleIn, &conMode);
	SetConsoleMode(handleIn, conMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

	// Read character and echo * to screen
	while (ReadConsoleA(handleIn, &ch, 1, &dwRead, NULL) && ch != '\r') {
		if (ch == '\b') {
			if (i > 0) {
				printf("\b \b");
				i--;
			}
		}
		else if (i < CRE_MAXLEN - 1 && ch != ' ') {
			password[i++] = ch;
			printf("*");
		}
	}
	password[i] = 0;

	// Restore original console mode
	SetConsoleMode(handleIn, conMode);
}

void showHeader() {
	system("cls");
	printf("******************************************\n");
	printf("****                                 *****\n");
	printf("****      Authentication Portal      *****\n");
	printf("****                                 *****\n");
	printf("******************************************\n");
	printf("\nHi! STATUS: ");
	if (isLoggedIn)
		printf("Logged In.\n");
	else
		printf("Not Logged In.\n");
	printf("\n");
}

/*
Show status message corresponding with status code to screen
[IN] statusCode:	the status code
[IN] opType:		the name of the operation performed
*/
void showStatusMsg(int statusCode, char* opType) {
	switch (statusCode) {
	case RES_OK:
		printf("%s successful.\n", opType);
		return;
	case RES_BAD_REQUEST:
		printf("Bad request error!\n");
		return;
	case RES_LI_WRONG_PASS:
		printf("Wrong password. Please try again!\n");
		return;
	case RES_LI_ALREADY_LI:
		printf("You have already logged in on this device!\n");
		return;
	case RES_LI_ACC_LOCKED:
		printf("Account is disabled. Please contact admin!\n");
		return;
	case RES_LI_ACC_NOT_FOUND:
		printf("Account not found. Do you have other id?\n");
		return;
	case RES_LI_ELSEWHERE:
		printf("Account already logged in on another device\n");
		return;
	case RES_LO_NOT_LI:
		printf("You did not log in!\n");
		return;
	case RES_RA_SID_NOT_FOUND:
		printf("No active session found.\n");
		return;
	}
}

void logInUI() {
	showHeader();
	printf("Your username and password should be 30 characters or less,\n");
	printf("and cannot contain spaces.\n");
	printf("Don't input over 30 characters, else it will be truncated.\n\n");

	bool valid;
	do {
		valid = true;
		printf("Username: ");
		scanf_s("%s", username, (unsigned)_countof(username));

		printf("Password: ");
		getPassword(password);
		if (strlen(username) == 0) {
			printf("\nUsername cannot be empty.\n\n");
			valid = false;
		}
		if (strlen(password) == 0) {
			printf("\nPassword cannot be empty.\n\n");
			valid = false;
		}
	} while (!valid);
	printf("\n\n");

	int ret = reqLogIn(username, password);
	if (ret == 1) {
		printf("\nClient will quit.\n");
		quit = true;
		Sleep(2000);
		return;
	}
	showStatusMsg(ret, "Log in");
	Sleep(2000);
}

void logOutUI() {
	char ch;
	int iRetVal;
	printf("\n");
	printf("Are you sure you want to log out? (Y/..): ");
	ch = _getch();
	if (ch == 'Y' || ch == 'y') {
		iRetVal = reqLogOut();
		if (iRetVal == 1) {
			printf("\nClient will quit.\n");
			quit = true;
			Sleep(2000);
			return;
		}
		showStatusMsg(iRetVal, "Log out");
		Sleep(2000);
		return;
	}
	printf("\nLog out aborted.");
	Sleep(2000);
}

void showLoggedInMenu() {
	showHeader();

	printf("You can choose the options below : \n");
	printf("\n");
	printf("1. Log out\n");
	printf("2. Quit\n");
	printf("\nChoose: ");

	char choice = '.';
	do {
		choice = _getch();
		switch (choice) {
		case '1':
			logOutUI();
			break;
		case '2':
			quit = true;
			return;
		default:
			printf("\nInvalid choice. Please choose again: ");
			choice = '.';
		}
	} while (choice == '.');
}

void showLoggedOutMenu() {
	showHeader();
	printf("1. Log in\n");
	printf("2. Quit\n");
	printf("\nChoose: ");

	char choice = '.';
	do {
		choice = _getch();

		switch (choice) {
		case '1':
			logInUI();
			break;
		case '2':
			quit = true;
			return;
		default:
			printf("\nInvalid choice. Please choose again: ");
			choice = '.';
		}
	} while (choice == '.');
}

void startUI() {
	// Try to reauthenticate
	// If fail, request new SID from server
	int iRetVal = reqReauth();
	showStatusMsg(iRetVal, "Reauthenticate");
	if (iRetVal == 1) {
		return;
	}
	else if (iRetVal != RES_OK && iRetVal != RES_RA_FOUND_NOTLI) {
		iRetVal = reqSid();
		showStatusMsg(iRetVal, "Request SID");
		if (iRetVal == 1) {
			printf("Program will exit.");
			return;
		}
	}
	Sleep(1000);
	
	// Main menu loop
	while (!quit) {
		if (isLoggedIn)
			showLoggedInMenu();
		else
			showLoggedOutMenu();
	}
}

