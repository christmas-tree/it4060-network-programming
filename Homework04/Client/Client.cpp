
#include "stdafx.h"
#include "uiUtils.h"

int main(int argc, char* argv[])
{
	// Validate parameters
	if (argc != 3) {
		printf("Wrong arguments! Please enter in format: client [ServerIpAddress] [ServerPortNumber]");
		return 1;
	}

	char* serverIpAddr = argv[1];
	short serverPortNumbr = atoi(argv[2]);

	// Initialize network
	int iRetVal = initConnection(serverIpAddr, serverPortNumbr);
	if (iRetVal == 1)
		return 1;
	
	// Open data file
	iRetVal = openFile("accData");
	if (iRetVal == 1)
		return 1;

	// Start program
	startUI();
	ioCleanup();

	return 0;
}