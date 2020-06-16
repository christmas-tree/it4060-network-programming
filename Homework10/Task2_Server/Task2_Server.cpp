
#include "stdafx.h"
#include "businessUtils.h"

#define MESSAGE_SIZE sizeof(MESSAGE)

//  Semaphores
HANDLE processQueueFull, processQueueEmpty, ioQueueFull, ioQueueEmpty;
HANDLE ioQueueMutualExclusion, processQueueMutualExclusion;

std::queue<PER_QUEUE_ITEM> fileIOQueue;

unsigned __stdcall serverWorkerThread(LPVOID CompletionPortID);
unsigned __stdcall serverBusinessThread(LPVOID none);

int main(int argc, char* argv[]) {
	int index;
	short port = 0;
	SOCKADDR_IN serverAddr;
	SOCKET listenSock, acceptSock;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	LPPER_HANDLE_DATA handleData[MAX_CLIENT] = { NULL };
	LPPER_IO_OPERATION_DATA ioData[MAX_CLIENT] = { NULL };
	DWORD transferredBytes;
	DWORD flags;
	WSADATA wsaData;

	// Validate parameters
	if (argc != 2) {
		printf("Wrong argument! Please enter in format: \"Task1_Server [ServerPortNumber]\"");
		return 1;
	}

	port = atoi(argv[1]);
	if (port <= 0) {
		printf("Wrong argument! Please enter in format: \"Task1_Server [ServerPortNumber]\"");
		return 1;
	}

	if (WSAStartup((2, 2), &wsaData) != 0) {
		printf("WSAStartup() failed with error %d\n", GetLastError());
		return 1;
	}

	// Initialize Semaphores & Mutexes
	processQueueFull = CreateSemaphore(NULL, 0, MAX_CLIENT, NULL);
	processQueueEmpty = CreateSemaphore(NULL, MAX_CLIENT, MAX_CLIENT, NULL);
	ioQueueFull = CreateSemaphore(NULL, 0, MAX_CLIENT, NULL);
	ioQueueEmpty = CreateSemaphore(NULL, MAX_CLIENT, MAX_CLIENT, NULL);
	ioQueueMutualExclusion = CreateMutex(NULL, false, NULL);
	processQueueMutualExclusion = CreateMutex(NULL, false, NULL);

	// Setup an I/O completion port
	if ((completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
		return 1;
	}

	GetSystemInfo(&systemInfo);
	// Create worker threads based on the number of processors available on the system.	
	for (int i = 0; i < (int)systemInfo.dwNumberOfProcessors; i++) {
		if (_beginthreadex(0, 0, serverWorkerThread, (void*)completionPort, 0, 0) == 0) {
			printf("Create worker thread failed with error %d\n", GetLastError());
			return 1;
		}
		else
			printf("Worker thread #%d created.\n", i + 1);
		if (_beginthreadex(0, 0, serverBusinessThread, NULL, 0, 0) == 0) {
			printf("Create business thread failed with error %d\n", GetLastError());
			return 1;
		}
		else
			printf("Business thread #%d created.\n", i + 1);
	}

	// Create a listening socket
	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("WSASocket() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSock, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		printf("bind() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	// Prepare socket for listening
	if (listen(listenSock, 1000) == SOCKET_ERROR) {
		printf("listen() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	// Accept connections
	while (1) {
		if ((acceptSock = WSAAccept(listenSock, NULL, NULL, NULL, 0)) == SOCKET_ERROR) {
			printf("WSAAccept() failed with error %d\n", WSAGetLastError());
			return 1;
		}

		// Thread synchronization
		WaitForSingleObject(ioQueueEmpty, INFINITE);
		WaitForSingleObject(ioQueueMutualExclusion, INFINITE);

		// Find an empty slot in handleData array
		for (index = 0; index < MAX_CLIENT && handleData[index] != NULL; index++);

		// Create a socket information structure to associate with the socket
		if ((handleData[index] = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA))) == NULL) {
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			continue;
		}

		// Associate the accepted socket with the original completion port
		handleData[index]->socket = acceptSock;
		printf("Socket number %d got connected...\n", acceptSock);
		if (CreateIoCompletionPort((HANDLE)acceptSock, completionPort, (DWORD)handleData[index], 0) == NULL) {
			printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
			continue;
		}

		// Create per I/O socket information structure to associate with the WSARecv call
		if ((ioData[index] = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL) {
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			continue;
		}

		ReleaseMutex(ioQueueMutualExclusion);
		ReleaseSemaphore(ioQueueFull, 1, NULL);

		ZeroMemory(&(ioData[index]->overlapped), sizeof(OVERLAPPED));
		ioData[index]->sentBytes = 0;
		ioData[index]->recvBytes = 0;
		ioData[index]->dataBuff.len = MESSAGE_SIZE;
		ioData[index]->dataBuff.buf = ioData[index]->buffer;
		ioData[index]->operation = RECEIVE;
		flags = 0;

		if (WSARecv(acceptSock, &(ioData[index]->dataBuff), 1, &transferredBytes,
			&flags, &(ioData[index]->overlapped), NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != ERROR_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				continue;
			}
		}
	}
	return 0;
}

/*
Main function for worker thread. Process network IO operations. Once it receives
a request, it puts the request into the processQueue.
[IN] completionPortID:	the ID of the completion port initiated.
*/
unsigned __stdcall serverWorkerThread(LPVOID completionPortID)
{
	HANDLE completionPort = (HANDLE)completionPortID;
	DWORD transferredBytes;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	PER_QUEUE_ITEM perQueueItem;
	DWORD flags;

	while (true) {
		if (GetQueuedCompletionStatus(completionPort, &transferredBytes,
			(LPDWORD)&perHandleData, (LPOVERLAPPED *)&perIoData, INFINITE) == 0) {
			printf("GetQueuedCompletionStatus() failed with error %d\n", GetLastError());
			continue;
		}

		// If an error has occurred on the socket, close socket and cleanup SOCKET_INFORMATION structure
		if (transferredBytes == 0) {
			// Synchronize threads
			WaitForSingleObject(ioQueueFull, INFINITE);
			WaitForSingleObject(ioQueueMutualExclusion, INFINITE);


			// Close socket
			printf("Closing socket %d\n", perHandleData->socket);
			if (closesocket(perHandleData->socket) == SOCKET_ERROR) {
				printf("closesocket() failed with error %d\n", WSAGetLastError());
				ReleaseMutex(ioQueueMutualExclusion);
				ReleaseSemaphore(ioQueueEmpty, 1, NULL);
				continue;
			}
			GlobalFree(perHandleData);
			GlobalFree(perIoData);

			ReleaseMutex(ioQueueMutualExclusion);
			ReleaseSemaphore(ioQueueEmpty, 1, NULL);

			continue;
		}
		
		// If the operation is RECEIVE, update the recvBytes field
		if (perIoData->operation == RECEIVE) {
			perIoData->recvBytes += transferredBytes;

			if (perIoData->recvBytes < sizeof(MESSAGE)) {
				ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
				perIoData->dataBuff.buf = perIoData->buffer + perIoData->recvBytes;
				perIoData->dataBuff.len = MESSAGE_SIZE - perIoData->recvBytes;

				if (WSARecv(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes,
					&flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						printf("Error %d! WSARecv() failed.\n", WSAGetLastError());
						continue;
					}
				}
			}
			else {
				// Push a perQueueItem to fileIOQueue for business threads to process
				perQueueItem.socket = perHandleData->socket;
				perQueueItem.perIoData = perIoData;
				memcpy(perQueueItem.buff, perIoData->buffer, MESSAGE_SIZE);

				WaitForSingleObject(processQueueEmpty, INFINITE);
				WaitForSingleObject(processQueueMutualExclusion, INFINITE);

				fileIOQueue.push(perQueueItem);

				ReleaseMutex(processQueueMutualExclusion);
				ReleaseSemaphore(processQueueFull, 1, NULL);

				// Post another WSARecv()
				perIoData->recvBytes = 0;
				flags = 0;
				ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
				perIoData->dataBuff.len = MESSAGE_SIZE;
				perIoData->dataBuff.buf = perIoData->buffer;

				if (WSARecv(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes,
					&flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						printf("WSARecv() failed with error %d\n", WSAGetLastError());
						continue;
					}
				}
			}
		}
		else if (perIoData->operation == SEND) {
			perIoData->sentBytes += transferredBytes;

			if (perIoData->sentBytes < MESSAGE_SIZE) {
				ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
				perIoData->dataBuff.buf = perIoData->buffer + perIoData->sentBytes;
				perIoData->dataBuff.len = MESSAGE_SIZE - perIoData->sentBytes;

				if (WSASend(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes,
					0, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						printf("Error %d! WSASend() failed.\n", WSAGetLastError());
						continue;
					}
				}
			}
			else {
				// No more bytes to send so post another WSARecv() request
				perIoData->operation = RECEIVE;
				perIoData->recvBytes = 0;
				flags = 0;
				ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
				perIoData->dataBuff.len = MESSAGE_SIZE;
				perIoData->dataBuff.buf = perIoData->buffer;

				if (WSARecv(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes,
					&flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						printf("WSARecv() failed with error %d\n", WSAGetLastError());
						continue;
					}
				}
			}
		}
	}
}

/*
Main function for request processing thread. Dequeues the process queue
and call processing functions.
[IN] none - not used.
*/
unsigned __stdcall serverBusinessThread(LPVOID none)
{
	DWORD transferredBytes;
	PER_QUEUE_ITEM perQueueItem;

	while (true) {
		WaitForSingleObject(processQueueFull, INFINITE);
		WaitForSingleObject(processQueueMutualExclusion, INFINITE);

		perQueueItem = fileIOQueue.front();
		fileIOQueue.pop();

		ReleaseMutex(processQueueMutualExclusion);
		ReleaseSemaphore(processQueueEmpty, 1, NULL);

		if (parseAndProcess(&perQueueItem)) {
			if (WSASend(perQueueItem.socket, &(perQueueItem.perIoData->dataBuff), 1, &transferredBytes,
				0, &(perQueueItem.perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					printf("Error %d! WSASend() failed.\n", WSAGetLastError());
					continue;
				}
			}
		}

	}
}