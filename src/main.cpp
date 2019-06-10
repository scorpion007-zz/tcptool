//==============================================================================
// Copyright 2019 Alex Budovski
// See LICENSE.TXT for license.
//==============================================================================

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdarg.h>

#define VER_MAJ 1
#define VER_MIN 0

#ifdef _DEBUG
#define DBGPRINT DebugPrint
#else
#define DBGPRINT
#endif

void
PrintError(const WCHAR *fmt, ...) {
  va_list args;
  fwprintf(stderr, L"error: ");
  va_start(args, fmt);
  vfwprintf(stderr, fmt, args);
  va_end(args);
}

void
DebugPrint(const WCHAR *fmt, ...) {
  va_list args;
  fwprintf(stderr, L"debug: ");
  va_start(args, fmt);
  vfwprintf(stderr, fmt, args);
  va_end(args);
}

typedef int WINSOCKERR;

WINSOCKERR
InitWinsock()
{
	WINSOCKERR err = 0;
	bool fInitWSA = false;
	WSADATA wsaData;
	
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err)
	{
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		PrintError(L"failed to init WSA: %d\n", err);
		goto exit;
	}

	fInitWSA = true;

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		PrintError(L"failed to obtain requested version\n");
		goto exit;
	}
	
exit:
	if (err && fInitWSA)
	{
		WSACleanup();
	}
	
	return err;
}

int main()
{
	WINSOCKERR err = 0;
	printf("tcptool version %d.%d\n", VER_MAJ, VER_MIN);
	
	err = InitWinsock();
	if (err)
	{
		PrintError(L"InitWinsock failed: %d\n", err);
		goto exit;
	}

	// Make a TCPv4 socket.
	//
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		PrintError(L"socket() failed: %d\n", err);
		goto exit;
	}
exit:
	return err;
}
