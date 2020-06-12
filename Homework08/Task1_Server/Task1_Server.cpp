
#include "stdafx.h"
#include "businessUtils.h"

//#pragma comment (lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
	// Validate parameter
	if (argc != 2) {
		printf("Wrong argument! Please enter in format: \"Task1_Server [ServerPortNumber]\"");
		return 1;
	}

	// Initialize network and load data
	int iRetVal = initData() + initConnection(atoi(argv[1]));
	if (iRetVal) {
		printf("\nProgram will exit.");
		return 1;
	}

	// Start server
	run();
	ioCleanup();
}