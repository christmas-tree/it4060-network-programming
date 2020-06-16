
#include "stdafx.h"
#include "uiUtils.h"

int main(int argc, char* argv[])
{
	// Validate parameters
	if (argc != 3) {
		printf("Wrong arguments! Please enter in format: Task2_Client [ServerIpAddress] [ServerPortNumber]");
		return 1;
	}

	char* serverIpAddr = argv[1];
	short serverPortNumber = atoi(argv[2]);

	// Initialize network
	int iRetVal = initConnection(serverIpAddr, serverPortNumber);
	if (iRetVal == 1)
		return 1;

	// Start program
	startUI();
	WSACleanup();

	return 0;
}