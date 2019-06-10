//==============================================================================
// Copyright 2019 Alex Budovski
// See LICENSE.TXT for license.
//==============================================================================

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <conio.h>

#define VER_MAJ 1
#define VER_MIN 0

#ifdef _DEBUG
#define DBGPRINT DebugPrint
#else
#define DBGPRINT
#endif

#define STRING_AND_CCH(x) x, _countof(x)
#define INVALID_SWITCH_VALUE assert(!"Invalid switch")

static const int x_MaxAddr = 128;

// Options.
//
struct TcpToolOpts
{
	// Connect address -- for clients.
	//
	WCHAR ConnectAddr[x_MaxAddr];

	// Either connect target or listen port.
	//
	short Port;
};

TcpToolOpts g_Opts;

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

WINSOCKERR
DumpSockOpts(
	_In_ SOCKET s)
{
	WINSOCKERR err = 0;
	int RcvBufSize = 0;
	int cbRcvBuf = sizeof(cbRcvBuf);
	err = getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&RcvBufSize, &cbRcvBuf);
	if (err)
	{
		err = WSAGetLastError();
		PrintError(L"getsockopt() failed: %d\n", err);
		goto exit;
	}
	
	int SndBufSize = 0;
	int cbSndBuf = sizeof(cbRcvBuf);
	err = getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&SndBufSize, &cbSndBuf);
	if (err)
	{
		err = WSAGetLastError();
		PrintError(L"getsockopt() failed: %d\n", err);
		goto exit;
	}

	printf("sock opts:\n");
	printf(" SO_RCVBUF: %d\n", RcvBufSize);
	printf(" SO_SNDBUF: %d\n", SndBufSize);
exit:
	return err;
}

_Check_return_ HRESULT
ConnectSocket(
	_In_ SOCKET s,
	_In_ PCWSTR connectAddr,
	short port)
{
	HRESULT hr = S_OK;
	WINSOCKERR err = 0;
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	
	// Parse the connect addr.
	//
	err = InetPton(AF_INET, connectAddr, &addr.sin_addr.s_addr);
	switch (err)
	{
		case 1:
			// Success.
			//
			break;
		case 0:
			PrintError(L"invalid IPv4 address: %s\n", connectAddr);
			hr = E_INVALIDARG;
			goto exit;
			break;
		case -1:
			err = WSAGetLastError();
			PrintError(L"InetPton() failed: %d\n", err);
			hr = E_FAIL;
			goto exit;
			break;
		default:
			INVALID_SWITCH_VALUE;
			break;
	}
	
	addr.sin_port = htons(port);
	err = connect(s, (SOCKADDR*)&addr, sizeof (addr));
	if (err)
	{
		err = WSAGetLastError();
		hr = E_FAIL;
		PrintError(L"connect() failed: %d\n", err);
		goto exit;
	}
exit:
	return hr;
}

_Check_return_ HRESULT
ListenSocket(
	_In_ SOCKET s,
	short port)
{
	HRESULT hr = S_OK;
	WINSOCKERR err = 0;
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	err = bind(s, (SOCKADDR*)&addr, sizeof (addr));
	if (err == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		hr = E_FAIL;
		PrintError(L"bind() failed: %d\n", err);
		goto exit;
	}
	
	err = listen(s, SOMAXCONN);
	if (err == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		hr = E_FAIL;
		PrintError(L"listen() failed: %d\n", err);
		goto exit;
	}
exit:
	return hr;
}

// TODO: factor out to common lib.
//
enum ArgType
{
	Arg_None,
	Arg_String,
	Arg_Int,
	Arg_Int64,
	
	// Add before this.
	//
	Arg_Last
};

struct ArgVal
{
	ArgType Type;
	union
	{
		PCWSTR StrVal;
		int IntVal;
		INT64 Int64Val;
	};
};

typedef _Check_return_ HRESULT (*ArgHdl)(_In_ const ArgVal* val);

struct Arg
{
	PCWSTR Name;
	PCWSTR ShortName;
	ArgType Type;
	ArgHdl Callback;
};

// Handle -connect.
//
static _Check_return_ HRESULT
hdlConnect(_In_ const ArgVal* val)
{
	StringCchCopy(STRING_AND_CCH(g_Opts.ConnectAddr), val->StrVal);
	return S_OK;
}

// Handle -port.
//
static _Check_return_ HRESULT
hdlPort(_In_ const ArgVal* val)
{
	g_Opts.Port = (short)val->IntVal;
	return S_OK;
}

static const Arg x_Args[] =
{
	{L"connect", L"c", Arg_String, hdlConnect},
	{L"port", L"p", Arg_Int, hdlPort},
};

_Check_return_ HRESULT
ProcessCommandLine(int argc, WCHAR** argv)
{
	HRESULT hr = S_OK;
	
	for (int i = 0; i < argc; ++i)
	{
		if (argv[i][0] == L'/' || argv[i][0] == L'-')
		{
			PCWSTR arg = argv[i]+1;
			// Look for matching arg.
			//
			for (int iArg = 0; iArg < _countof(x_Args); ++iArg)
			{
				if (!wcscmp(x_Args[iArg].Name, arg) || !wcscmp(x_Args[iArg].ShortName, arg))
				{
					PCWSTR wszArgVal = nullptr;
					ArgVal argVal;
					int cConv = 0;
					
					// Parse the val if necessary.
					//
					if (x_Args[iArg].Type != Arg_None)
					{
						if (i + 1 >= argc)
						{
							// Not enough args.
							//
							PrintError(L"arg '%s' requires param\n", arg);
							hr = E_INVALIDARG;
							goto exit;
						}

						// Consume one arg.
						//
						wszArgVal = argv[++i];
					}
					
					argVal.Type = x_Args[iArg].Type;
					
					switch (x_Args[iArg].Type)
					{
					case Arg_String:
						argVal.StrVal = wszArgVal;
						break;
					case Arg_Int:
						cConv = swscanf_s(wszArgVal, L"%i", &argVal.IntVal);
						if (cConv != 1)
						{
							PrintError(L"arg '%s' requires int. Got %s\n", wszArgVal);
							hr = E_INVALIDARG;
							goto exit;
						}
						break;
					case Arg_Int64:
						cConv = swscanf_s(wszArgVal, L"%I64i", &argVal.Int64Val);
						if (cConv != 1)
						{
							PrintError(L"arg '%s' requires int64. Got %s\n", wszArgVal);
							hr = E_INVALIDARG;
							goto exit;
						}
						break;
					default:
						assert(x_Args[iArg].Type == Arg_None);
						break;
					}
					hr = x_Args[iArg].Callback(&argVal);
					if (FAILED(hr))
					{
						goto exit;
					}
				}
			}
		}
	}
exit:
	return hr;
}

int wmain(int argc, WCHAR** argv)
{
	char buf[128];
	
	WINSOCKERR err = 0;

	HRESULT hr = S_OK;
	
	printf("tcptool version %d.%d\n", VER_MAJ, VER_MIN);
	
	err = InitWinsock();
	if (err)
	{
		PrintError(L"InitWinsock() failed: %d\n", err);
		goto exit;
	}

	hr = ProcessCommandLine(argc, argv);
	if (FAILED(hr))
	{
		PrintError(L"ProcessCommandLine() failed: %x\n", hr);
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

	if (g_Opts.ConnectAddr[0])
	{
		hr = ConnectSocket(s, g_Opts.ConnectAddr, g_Opts.Port);
		if (FAILED(hr))
		{
			PrintError(L"ConnectSocket() failed: %x\n", hr);
			goto exit;
		}

		while (true)
		{
			printf("press any key to post a send\n");
			int ch = _getch();
			// printf("got %d (%c)\n", ch, ch);
			static const int x_ETX = 3;  // ASCII End-Of-Text (CTRL-C).
			if (ch == x_ETX)
			{
				printf("got CTRL-C, exiting.\n");
				break;
			}
			
			int ret = send(s, buf, sizeof(buf), 0);
			if (ret == SOCKET_ERROR)
			{
				err = WSAGetLastError();
				PrintError(L"send() failed: %d\n", err);
				goto exit;
			}
		}
	}
	else
	{
		if (!g_Opts.Port)
		{
			PrintError(L"port required to listen\n");
			goto exit;
		}

		// Listen is assumed in absense of -connect.
		//
		hr = ListenSocket(s, g_Opts.Port);
		if (FAILED(hr))
		{
			PrintError(L"ListenSocket() failed: %x\n", hr);
			goto exit;
		}

		printf("listening on port %d. Waiting for client.\n", g_Opts.Port);
		SOCKET acceptSoc = accept(s, nullptr, nullptr);
		if (acceptSoc == INVALID_SOCKET)
		{
			err = WSAGetLastError();
			PrintError(L"accept() failed: %d\n", err);
			goto exit;
		}
		
		printf("client connected.\n");

		while (true)
		{
			printf("posting one recv.\n");
			int ret = recv(acceptSoc, buf, sizeof(buf), 0);
			if (!ret)
			{
				// conn was gracefully closed.
				//
				break;
			}

			if (ret == SOCKET_ERROR)
			{
				err = WSAGetLastError();
				PrintError(L"recv() failed: %d\n", err);
				goto exit;
			}
		}
	}

	err = DumpSockOpts(s);
	if (err)
	{
		PrintError(L"DumpSockOpts() failed: %d\n", err);
		goto exit;
	}
exit:
	if (FAILED(hr))
	{
		return hr;
	}
	else
	{
		return err;
	}
}
