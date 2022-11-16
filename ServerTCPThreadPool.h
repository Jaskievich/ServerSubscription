#pragma once


#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include <deque>
#include <vector>

using namespace std;

#pragma comment (lib, "Ws2_32.lib")

/*
Вариант серврера - accep в одном потоке, очередь сокетов, пул потоков для обработки клиентов
*/

typedef char TCHAR_ADDRESS[16];
//#define MAX_BUF 1024


static int ReadAll(SOCKET ClientSocket, char *p_buff, int size)
{
	int iResult = 0;
	while (size){
		iResult = recv(ClientSocket, p_buff, size, 0);
		if (iResult <= 0) break;
		p_buff += iResult;
		size -= iResult;
	}
	return iResult;
}

static 	int _SendAll(SOCKET ClientSocket, char *p_buff, int size)
{
	int iResult = 0;
	while (size) {
		iResult = send(ClientSocket, p_buff, size, 0);
		if (iResult < 0) break;
		size -= iResult;
		p_buff += iResult;
	}
	return iResult;
}

#define MAX_BUF 1024

static int SendAll(SOCKET ClientSocket, char *p_buff, int size)
{
	int iResult = 0;
	while (MAX_BUF < size) {
		iResult = _SendAll(ClientSocket, p_buff, MAX_BUF);
		if (iResult < 0) break;
		p_buff += MAX_BUF;
		size -= MAX_BUF;
	}
	iResult = _SendAll(ClientSocket, p_buff, size);
	return iResult;
}


struct TBuffHeader
{
	int cmd;		// Код команды

	int size;		// Размер дополнительного буфера

	DWORD param;	// Дополнительный параметр

};

struct TSocketData
{
	SOCKET ClientSocket;

	TBuffHeader header;

	char *p_buff;

	TSocketData():p_buff(NULL), ClientSocket(NULL)
	{}

	void ClearBuff()
	{
		ZeroMemory(&header, sizeof(TBuffHeader));
		p_buff = NULL;
	}

	~TSocketData()
	{
		closesocket(ClientSocket);
	}


};


//struct TBuff
//{
//	int cmd;
//	char buff[MAX_BUF];	
//
//	TBuff()
//	{
//		ClearBuff();
//	}
//
//	void ClearBuff()
//	{
//		ZeroMemory(buff, MAX_BUF);
//	}
//
//
//
//	int ReadBuff(SOCKET ClientSocket)
//	{
//		DWORD param = 0;
//		int iResult = recv(ClientSocket, (char *)&param, sizeof(DWORD), 0);
//		cmd = HIWORD(param);
//		int size = LOWORD(param);
//		char *p_buff = buff;
//		while (size)
//		{
//			iResult = recv(ClientSocket, p_buff, size, 0);
//			if (iResult <= 0) return iResult;
//			p_buff += iResult;
//			size -= iResult;
//		}
//		return 1;
//	}
//};


enum E_Result
{
	e_OK,		// данные полностью получены
	e_Next,		// Есть еще порция данных
	e_Err		// Ошибка при обработке данных
};

/*
	Длля создания объектов пользователя
*/
//class CObjectHandleClient
//{
//public: 
//	static CObjectHandleClient *inst;
//
//	~CObjectHandleClient()
//	{
//		inst = nullptr;
//	}
//};
//
//CObjectHandleClient *CObjectHandleClient::inst = nullptr;

/*
	Прототип обработчика команд
	Необходимр реализовать
*/
//E_Result HandleCmd(TSocketData &buff_cmd, TCHAR_ADDRESS address, CObjectHandleClient **objClient  );

E_Result HandleCmd(TSocketData &buff_cmd, TCHAR_ADDRESS address);



VOID CALLBACK MyWorkCallbackHandleClient(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work);

VOID CALLBACK MyWorkCallbackHandleClientExt(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work);


class CCriticalSection
{
private:
	CRITICAL_SECTION &criticalSection;
public:
	CCriticalSection(CRITICAL_SECTION &_criticalSection) :criticalSection(_criticalSection)
	{
		Enter();
	}
	void Enter()
	{
		EnterCriticalSection(&criticalSection);
	}

	void Leave()
	{
		LeaveCriticalSection(&criticalSection);
	}

	~CCriticalSection()
	{
		Leave();
	}
};


/*
	Класс отвечающий за TCP - сервер на основе пула потоков
*/
class CServerTCPThreadpool
{
protected:

	WSADATA wsaData;
	SOCKET ListenSocket;
	PTP_WORK work;

	int InitWinSock()
	{
		// Initialize Winsock
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			printf("WSAStartup failed with error: %d\n", iResult);
			return 1;
		}

		struct addrinfo *result = NULL;
		struct addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the server address and port
		iResult = getaddrinfo(NULL, port, &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			WSACleanup();
			return 1;
		}

		// Create a SOCKET for connecting to server
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			return 1;
		}

		// Setup the TCP listening socket
		iResult = ::bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("bind failed with error: %d\n", WSAGetLastError());
			freeaddrinfo(result);
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		freeaddrinfo(result);
		iResult = listen(ListenSocket, SOMAXCONN);
		if (iResult == SOCKET_ERROR) {
			printf("listen failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		return 0;
	}

public:

	std::deque<pair<SOCKET, TCHAR_ADDRESS> > deqSocket;
	CRITICAL_SECTION mCriticalSection;

	const char		*port;

public:

	CServerTCPThreadpool(const char	*_port) :port(_port)
	{
		ListenSocket = INVALID_SOCKET;
		InitializeCriticalSection(&mCriticalSection);
	}

	bool GetSocket_Address(SOCKET &sock, TCHAR_ADDRESS adress)
	{
		CCriticalSection criticalSection(mCriticalSection);
		if (deqSocket.empty()) return false;
		sock = deqSocket.front().first;
		strncpy(adress, deqSocket.front().second, sizeof(TCHAR_ADDRESS));
		deqSocket.pop_front();
		adress[sizeof(TCHAR_ADDRESS) - 1] = 0;
		return true;
	}

	/*
		Инициализация для работы с простой передачей данных по сети  (не предполагает передачу больших объемов)
	*/
	int Init()
	{
		work = CreateThreadpoolWork(MyWorkCallbackHandleClient, this, NULL);
		if (NULL == work) {
			_tprintf(_T("CreateThreadpoolWork failed. LastError: %u\n"),
				GetLastError());
		}
		
	//	SetThreadpoolThreadMaximum(pool_dat->pool, 256);
		return InitWinSock();
	}

	void Run()
	{
		SOCKET socket = INVALID_SOCKET;
		sockaddr_in client;
		int len_client = sizeof(sockaddr);
		pair<SOCKET, TCHAR_ADDRESS> p;
		for ( ;; ) {

			socket = accept(ListenSocket, (sockaddr *)&client, &len_client);
			EnterCriticalSection(&mCriticalSection);
			p.first = socket;
			strncpy(p.second, inet_ntoa(client.sin_addr), 15);
			deqSocket.push_back(p);
			LeaveCriticalSection(&mCriticalSection);
			if (socket == INVALID_SOCKET) {
				printf("accept failed with error: %d\n", WSAGetLastError());
				continue;
			}
			// Receive until the peer shuts down the connection
			SubmitThreadpoolWork(work);
		}

		WaitForThreadpoolWorkCallbacks(work, false);

		closesocket(ListenSocket);
	}

	~CServerTCPThreadpool()
	{
		DeleteCriticalSection(&mCriticalSection);
		closesocket(ListenSocket);
		WSACleanup();
	}

};


/*
	Прототип функции вывода
	Необходимр реализовать
*/
void _printf(const char *mess, int val);



VOID CALLBACK MyWorkCallbackHandleClient(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
	// Instance, Parameter, and Work not used in this example.
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(Work);
	CServerTCPThreadpool *serverTCPThreadpool = (CServerTCPThreadpool *)Parameter;
	TCHAR_ADDRESS ip_address_client;
	TSocketData buff_cmd;
	if (!serverTCPThreadpool->GetSocket_Address(buff_cmd.ClientSocket, ip_address_client)) return;
	int iResult = 0;
	const char *mess_send = "ok";
	E_Result res = e_OK;
	do
	{
		buff_cmd.ClearBuff();
		iResult = ReadAll(buff_cmd.ClientSocket, (char *)&buff_cmd.header, sizeof(buff_cmd.header));
		if (iResult < 1) {
			_printf("recv headr failed with error: %d\n", WSAGetLastError());	
			return;
		}	
		// Обработка строки-команды
		res = HandleCmd(buff_cmd, ip_address_client);
	/*	if( buff_cmd.header.size && res != e_Err ){
			iResult = ReadAll(buff_cmd.ClientSocket, buff_cmd.p_buff, buff_cmd.header.size);
			if (iResult < 1) {
				_printf("recv buff failed with error: %d\n", WSAGetLastError());
				return;
			}
		}*/

	} while (res == e_Next);
	// Ошибка 
	if (res == e_Err)	mess_send = "er";
	iResult = send(buff_cmd.ClientSocket, mess_send, 2, 0);
	if (iResult == SOCKET_ERROR) {
		_printf("send failed with error: %d\n", WSAGetLastError());
		return;
	}
	iResult = shutdown(buff_cmd.ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) _printf("shutdown failed with error: %d\n", WSAGetLastError());
	//	Sleep(1000);
}



/*
	Класс для работы с расширенной передачей данных по сети (предполагает передачу больших объемов)
*/
template<class T>
class CServerTCPThreadpoolExt :public CServerTCPThreadpool
{
public:
	
	CServerTCPThreadpoolExt(const char	*_port) : CServerTCPThreadpool(_port)
	{
	}

	int Init()
	{
		work = CreateThreadpoolWork(MyWorkCallbackHandleClientExt<T>, this, NULL);
		if (NULL == work) {
			_tprintf(_T("CreateThreadpoolWork failed. LastError: %u\n"),
				GetLastError());
		}
		return InitWinSock();
	}
};


template<class T>
VOID CALLBACK MyWorkCallbackHandleClientExt(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
	// Instance, Parameter, and Work not used in this example.
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(Work);
	CServerTCPThreadpoolExt<T> *serverTCPThreadpool = (CServerTCPThreadpoolExt<T> *)Parameter;
	TCHAR_ADDRESS ip_address_client;
	TSocketData buff_cmd_in;
	if (!serverTCPThreadpool->GetSocket_Address(buff_cmd_in.ClientSocket, ip_address_client)) return;
	int iResult = 0;
	const char *mess_send = "ok";
	E_Result res = e_OK;
	T objectClient;
	do
	{
		buff_cmd_in.ClearBuff();
		iResult = ReadAll(buff_cmd_in.ClientSocket, (char *)&buff_cmd_in.header, sizeof(buff_cmd_in.header));
		if (iResult < 1) {
			_printf("recv headr failed with error: %d\n", WSAGetLastError());
			return;
		}
		// Обработка строки-команды
		res = objectClient.BeginHandleCmd(buff_cmd_in, ip_address_client);
		if (buff_cmd_in.header.size && res != e_Err) {
			iResult = ReadAll(buff_cmd_in.ClientSocket, buff_cmd_in.p_buff, buff_cmd_in.header.size);
			if (iResult < 1) {
				_printf("recv buff failed with error: %d\n", WSAGetLastError());
				return;
			}
		}
	} while (res == e_Next);

	// Ошибка 
	if (res != e_Err) {
		if (objectClient.EndHandleCmd(buff_cmd_in)) {
			iResult = SendAll(buff_cmd_in.ClientSocket, (char *)&buff_cmd_in.header, sizeof(TBuffHeader));
			iResult = SendAll(buff_cmd_in.ClientSocket, buff_cmd_in.p_buff, buff_cmd_in.header.size);
		}
	}
	else iResult = send(buff_cmd_in.ClientSocket, "er", 2, 0);

	if (iResult == SOCKET_ERROR) {
		_printf("send failed with error: %d\n", WSAGetLastError());
		return;
	}
	iResult = shutdown(buff_cmd_in.ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) _printf("shutdown failed with error: %d\n", WSAGetLastError());
	//	Sleep(1000);
}


//VOID CALLBACK MyWorkCallbackHandleClientExt(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
//{
//	// Instance, Parameter, and Work not used in this example.
//	UNREFERENCED_PARAMETER(Instance);
//	UNREFERENCED_PARAMETER(Work);
//	CServerTCPThreadpoolExt *serverTCPThreadpool = (CServerTCPThreadpoolExt *)Parameter;
//	TCHAR_ADDRESS ip_address_client;
//	TSocketData buff_cmd;
//	if (!serverTCPThreadpool->GetSocket_Address(buff_cmd.ClientSocket, ip_address_client)) return;
//	int iResult = 0;
//	const char *mess_send = "ok";
//	E_Result res = e_OK;
//	CObjectHandleClient *objectClient = nullptr;
//	do
//	{
//		buff_cmd.ClearBuff();
//		iResult = ReadAll(buff_cmd.ClientSocket, (char *)&buff_cmd.header, sizeof(buff_cmd.header));
//		if (iResult < 1) {
//			_printf("recv headr failed with error: %d\n", WSAGetLastError());
//			if (objectClient) delete objectClient;
//			return;
//		}
//		// Обработка строки-команды
//		res = HandleCmd(buff_cmd, ip_address_client, &objectClient);
//		if (buff_cmd.header.size && res != e_Err) {
//			iResult = ReadAll(buff_cmd.ClientSocket, buff_cmd.p_buff, buff_cmd.header.size);
//			if (iResult < 1) {
//				_printf("recv buff failed with error: %d\n", WSAGetLastError());
//				if (objectClient) delete objectClient;
//				return;
//			}
//		}
//	} while (res == e_Next);
//	// Ошибка 
//	if (res == e_Err)	mess_send = "er";
//	iResult = send(buff_cmd.ClientSocket, mess_send, 2, 0);
//	if (iResult == SOCKET_ERROR) {
//		_printf("send failed with error: %d\n", WSAGetLastError());
//		if (objectClient) delete objectClient;
//		return;
//	}
//	iResult = shutdown(buff_cmd.ClientSocket, SD_SEND);
//	if (iResult == SOCKET_ERROR) _printf("shutdown failed with error: %d\n", WSAGetLastError());
//	//	Sleep(1000);
//	if (objectClient) delete objectClient;
//}


//VOID CALLBACK MyWorkCallbackHandleClient(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
//{
//	// Instance, Parameter, and Work not used in this example.
//	UNREFERENCED_PARAMETER(Instance);
//	UNREFERENCED_PARAMETER(Work);
//	CServerTCPThreadpool *serverTCPThreadpool = (CServerTCPThreadpool *)Parameter;
//	TCHAR_ADDRESS ip_address_client;
//	SOCKET ClientSocket;
//	if (!serverTCPThreadpool->GetSocket_Address(ClientSocket, ip_address_client)) return;
//	int iResult = 0;
//	const char *mess_send = "ok";
//	E_Result res = e_OK;
//	TBuff buff_cmd;
//	do	
//	{
//		buff_cmd.ClearBuff();
//		iResult = buff_cmd.ReadBuff(ClientSocket);
//		if (iResult < 1) 	{
//			_printf("recv failed with error: %d\n", WSAGetLastError());
//			closesocket(ClientSocket);
//			return;
//		}
//		// Обработка строки-команды
//		res = HandleCmd(buff_cmd, ip_address_client);
//	} while (res == e_Next);
//	// Ошибка 
//	if (res == e_Err)	mess_send = "er";
//	iResult = send(ClientSocket, mess_send, 2, 0);
//	if (iResult == SOCKET_ERROR) 	{
//		_printf("send failed with error: %d\n", WSAGetLastError());
//		closesocket(ClientSocket);
//		return;
//	}
//	iResult = shutdown(ClientSocket, SD_SEND);
//	if (iResult == SOCKET_ERROR) _printf("shutdown failed with error: %d\n", WSAGetLastError());
//	closesocket(ClientSocket);
//	//	Sleep(1000);
//	return;
//}