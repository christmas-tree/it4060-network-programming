
#include "stdafx.h"
#include "businessUtils.h"

//#pragma comment (lib, "Ws2_32.lib")

int main(int argc, char* argv[]) {
	// Validate parameter
	if (argc != 2) {
		printf("Wrong argument! Please enter in format: server [ServerPortNumber]");
		return 1;
	}

	// Initialize network and load data
	int iRetVal = initConnection(atoi(argv[1]));
	if (iRetVal) {
		printf("\nProgram will exit.");
		return 1;
	}

	// Start server
	run();
	networkCleanup();
}