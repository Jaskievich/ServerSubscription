// ServerSubscription.cpp : Defines the entry point for the console application.
//



#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <mstcpip.h>

#include <deque>
#include <vector>
#include <time.h>
#include <process.h>

using namespace std;

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"
#define PORT_UDP 27017
#define N_SEND 10


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

CRITICAL_SECTION gCriticalSection_CONSOLE;




long count_rec = 0;

void _printf_c(const char *mess, int val)
{
	EnterCriticalSection(&gCriticalSection_CONSOLE);
	count_rec++;
	printf("%i - ", count_rec);
	printf(mess, val);
	LeaveCriticalSection(&gCriticalSection_CONSOLE);
}

void _printf_c(const char *mess)
{
	EnterCriticalSection(&gCriticalSection_CONSOLE);
	count_rec++;
	printf("%i - ", count_rec);
	printf(mess);
	LeaveCriticalSection(&gCriticalSection_CONSOLE);
}

void _printf(const char *mess, int val)
{
	EnterCriticalSection(&gCriticalSection_CONSOLE);
	printf(mess, val);
	LeaveCriticalSection(&gCriticalSection_CONSOLE);
}

void _printf(const char *mess)
{
	EnterCriticalSection(&gCriticalSection_CONSOLE);
	printf(mess);
	LeaveCriticalSection(&gCriticalSection_CONSOLE);
}

typedef char TCHAR_ADDRESS[16];
#define BUFLEN 3

/*
	Класс UDP - клиент
*/
class CUDPClient
{
private:

	int m_socket;

	struct sockaddr_in si_other;

	char buf[BUFLEN];

	int cod_err;

public:
	CUDPClient(/*TCHAR_ADDRESS _ip_address*/)
	{
		memset((char *)&si_other, 0, sizeof(si_other));
		si_other.sin_family = AF_INET;
		si_other.sin_port = htons(PORT_UDP);
	/*	si_other.sin_addr.S_un.S_addr = inet_addr(_ip_address);*/
	}

	~CUDPClient()
	{
		closesocket(m_socket);
	
	}

	int Init()
	{		
		if ((m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)	{
			_printf(" socket() failed with error code : %d", WSAGetLastError());
			return 1;
		}
		DWORD val = 10000;
		setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&val, sizeof DWORD); //без этого вызова висим вечно
	//	WSAIoctl(m_socket, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, reinterpret_cast<DWORD*>(&dwSize), NULL, NULL);
		return 0;
	}

	int Send(const TCHAR_ADDRESS _ip_address)
	{
		si_other.sin_addr.S_un.S_addr = inet_addr(_ip_address);
		int i = 0, slen = sizeof(si_other);
		for (; i < 3; ++i) {
			cod_err = 0;
			//send the message
			if (sendto(m_socket, "ok", 2, 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)	{
				cod_err = WSAGetLastError();
				_printf("sendto() failed with error code : %d\n" , cod_err);
				continue;
			}
			memset(buf, 0, BUFLEN);
			//try to receive some data, this is a blocking call			
			if (recvfrom(m_socket, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == SOCKET_ERROR)	{
				cod_err = WSAGetLastError();
				_printf("recvfrom() failed with error code : %d\n" , cod_err);
				continue;
			}
			if (buf[0] == 'o' && buf[1] == 'k') break;
			cod_err = 1;
		}
		return i >= 3;
	}

};



unsigned __stdcall Handle_event_thread(LPVOID lpvParam);
/*
	Класс для работы с подписчиков
*/
struct CSubscriber
{
	vector<string> vIP_Address;
	char name_event[MAX_PATH];
	vector<string> vName_alias_event;
	HANDLE m_hEventClose;
	HANDLE hThread;
	SRWLOCK m_lock;

	CSubscriber(const char *_name_event)
	{
		strncpy(name_event, _name_event, MAX_PATH - 1);
		name_event[MAX_PATH - 1] = 0;
		m_hEventClose = CreateEvent(NULL, TRUE, FALSE, NULL);
		InitializeSRWLock(&m_lock);
	}

	void Add_Alias(const char *name_event) 
	{
		AcquireSRWLockExclusive(&m_lock);
		vName_alias_event.push_back(name_event);
		ReleaseSRWLockExclusive(&m_lock);
	}

	void Del_Alias(const char *name_event) 
	{
		AcquireSRWLockExclusive(&m_lock);
		vector<string>::iterator it;
		for (it = vName_alias_event.begin(); it != vName_alias_event.end();it++)
			if (*it == name_event) {
				vName_alias_event.erase(it);
				return;
			}
		ReleaseSRWLockExclusive(&m_lock);
	}

	void Add_Ip_address(const TCHAR_ADDRESS _ip_address) 
	{
		//	strcpy(ip_address, _ip_address);
		AcquireSRWLockExclusive(&m_lock);
		vIP_Address.push_back(_ip_address);
		ReleaseSRWLockExclusive(&m_lock);
	}

	void Start()	
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, &Handle_event_thread, this, 0/* CREATE_SUSPENDED*/, NULL);
	}

	void Stop()	{
		SetEvent(m_hEventClose);
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		hThread = NULL;
	}

	int _IndexIp_adress(const TCHAR_ADDRESS _ip_address)
	{
		for (int i = 0; i < vIP_Address.size(); ++i)
			if (vIP_Address[i] == _ip_address) return i;
		return -1;
	}
	
	int IndexIp_adress(const TCHAR_ADDRESS _ip_address)
	{
		AcquireSRWLockShared(&m_lock);
		int index = _IndexIp_adress(_ip_address);
		ReleaseSRWLockShared(&m_lock);
		return index;
	}

	void DelIpAdress(const TCHAR_ADDRESS _ip_address)
	{
		AcquireSRWLockExclusive(&m_lock);
		int index = _IndexIp_adress(_ip_address);
		if (index != -1) {
			vector<string>::iterator it = vIP_Address.begin() + index;
			if (it < vIP_Address.end()) vIP_Address.erase(it);
		}
		ReleaseSRWLockExclusive(&m_lock);
	}

};


/*
	Класс для работы со списком подписчиков
*/

class CListSubscriber
{
private:
	
	SRWLOCK m_lock;
	vector<CSubscriber *> vSubscriber;

private:

	inline int _IndexSubScriberByNameEvent(const char *name_event)
	{		
		for (int i = 0; i < vSubscriber.size(); ++i)
			if (strcmp(vSubscriber[i]->name_event, name_event) == 0) return i;
		return -1;
	}

public:
	CListSubscriber()
	{
		InitializeSRWLock(&m_lock);
	}

	~CListSubscriber()
	{
		vector<CSubscriber *>::iterator it = vSubscriber.begin();
		for (; it != vSubscriber.end(); it++)	delete *it;
	}

	CSubscriber *operator[](int index)
	{
		return vSubscriber[index];
	}

	CSubscriber * SubScriberByNameEvent(const char *name_event)
	{
		CSubscriber *res = NULL;
		AcquireSRWLockShared(&m_lock);
		int index = _IndexSubScriberByNameEvent(name_event);
		if( index > -1)	res = vSubscriber[index];
		ReleaseSRWLockShared(&m_lock);
		return res;
	}

	void AddSubscriber(CSubscriber *subscriber)
	{
		AcquireSRWLockExclusive(&m_lock);
		vSubscriber.push_back(subscriber);
		ReleaseSRWLockExclusive(&m_lock);
	}

	void DelSubscriber(const char *name_event)
	{
		AcquireSRWLockExclusive(&m_lock);
		int i = _IndexSubScriberByNameEvent(name_event);
		if (i > -1) {
			vector<CSubscriber *>::iterator it = vSubscriber.begin() + i;
			if (it < vSubscriber.end()) vSubscriber.erase(it);
		}
		ReleaseSRWLockExclusive(&m_lock);
	}


};


/*
	Вариант серврера - accep в одном потоке, очередь сокетов, пул потоков для обработки клиентов
*/


VOID CALLBACK MyWorkCallbackHandleClient(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work);


enum E_Comm_Subcript
{
	E_ADD,
	E_DEL,
	E_ADD_ALIAS,
	E_DEL_ALIAS
};


struct TBuff
{
	E_Comm_Subcript cmd;
	char buff[MAX_PATH];

	TBuff()
	{
		ZeroMemory(buff, MAX_PATH);
	}

	int ReadBuff(SOCKET ClientSocket )
	{
		DWORD param = 0;
		int iResult = recv(ClientSocket, (char *)&param, sizeof(DWORD), 0);
		cmd = (E_Comm_Subcript)HIWORD(param);
		int size = LOWORD(param);
		while (size)
		{
			iResult = recv(ClientSocket, buff, MAX_PATH, 0);
			if (iResult <= 0) return iResult;
			size -= iResult;
		}
		return 1;
	}

};

/*
	Класс отвечающий за TCP - сервер на основе пула потоков
*/
class CServerTCPThreadpool
{
protected:
	
	WSADATA wsaData;
	SOCKET ListenSocket ;
	PTP_WORK work;
public:
	std::deque<pair<SOCKET, TCHAR_ADDRESS> > deqSocket;
	CRITICAL_SECTION mCriticalSection;

public:
	
	CServerTCPThreadpool()
	{
		ListenSocket = INVALID_SOCKET;
		InitializeCriticalSection(&mCriticalSection);
	}

	int Init()
	{
		work = CreateThreadpoolWork(MyWorkCallbackHandleClient, this, NULL);
		if (NULL == work) {
			_tprintf(_T("CreateThreadpoolWork failed. LastError: %u\n"),
				GetLastError());
		}


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
		iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
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
		iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
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

	void Run()
	{
		SOCKET ClientSocket = INVALID_SOCKET;
		sockaddr_in client;
		int len_client = sizeof(sockaddr);
		pair<SOCKET, TCHAR_ADDRESS> p;
		for (;; ) {

			ClientSocket = accept(ListenSocket, (sockaddr *)&client, &len_client);
			EnterCriticalSection(&mCriticalSection);
			p.first = ClientSocket;
			strncpy(p.second, inet_ntoa(client.sin_addr), 15);
			deqSocket.push_back(p);
			LeaveCriticalSection(&mCriticalSection);
			if (ClientSocket == INVALID_SOCKET) {
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


CListSubscriber g_ListSubscriber;  // гдобальнай список

// Обработка комманд
int HandleCmd(TBuff &buff_cmd, TCHAR_ADDRESS address)
{
	if (!buff_cmd.buff[0]) return 1;
	CSubscriber *subs = g_ListSubscriber.SubScriberByNameEvent(buff_cmd.buff);
	switch (buff_cmd.cmd)
	{
	case E_ADD: {		
		if (subs == NULL) {
			subs = new CSubscriber(buff_cmd.buff);
			subs->Add_Ip_address(address);
			// Добавить в список
			g_ListSubscriber.AddSubscriber(subs);
			// Запустить обработку событий
			subs->Start();
		}
		else {
			int ind = subs->IndexIp_adress(address);
			if (ind == -1) subs->Add_Ip_address(address);
		}
		break;
	}
	case E_DEL:
		if (subs) {
			if (subs->vIP_Address.size() == 1)	{
				subs->Stop();
				g_ListSubscriber.DelSubscriber(subs->name_event);
			}
			else subs->DelIpAdress(address);
		}
		break;
	case E_ADD_ALIAS:
		if (subs) {
			subs->Stop();
			char *p = strtok(buff_cmd.buff, ";");
			while (p) {
				subs->Add_Alias(p);
				p = strtok(NULL, ";");
			}
			subs->Start();
		}
		break;
	case E_DEL_ALIAS:
		if (subs) {
			subs->Stop();
			subs->Del_Alias(buff_cmd.buff);
			subs->Start();
		}

		break;
	}
	return 0;
}



unsigned __stdcall Handle_event_thread(LPVOID lpvParam)
{
	CSubscriber *subscriber = (CSubscriber *)lpvParam;
	HANDLE hEventChange = OpenEvent(SYNCHRONIZE, TRUE, subscriber->name_event);
	if (!hEventChange) {
		_printf("OpenEvent error: %d\n", GetLastError());
		return 0;
	}
	CUDPClient UDPClient;
	if (UDPClient.Init()) return 0;
	DWORD iRes = 0;
	HANDLE arr_handele[2] = { hEventChange, subscriber->m_hEventClose };
	while (true) {
		iRes = WaitForMultipleObjects(2, arr_handele, FALSE, INFINITE);
		if (iRes == WAIT_OBJECT_0) {
			// послать udp - пинг по адресу
			//UDPClient.Send(subscriber->ip_address);
			AcquireSRWLockShared(&subscriber->m_lock);
			for (int i = 0; i < subscriber->vIP_Address.size(); ++i)
				if (UDPClient.Send(subscriber->vIP_Address[i].c_str())) {
					ReleaseSRWLockShared(&subscriber->m_lock);
					// удалить Ip-adress
					subscriber->DelIpAdress(subscriber->vIP_Address[i].c_str());
					AcquireSRWLockShared(&subscriber->m_lock);
					if (subscriber->vIP_Address.size() == 0) {// Удалить из списка подписчиков
						g_ListSubscriber.DelSubscriber(subscriber->name_event);
						ReleaseSRWLockShared(&subscriber->m_lock);
						delete subscriber;
						return 0;
					}
				}
			ReleaseSRWLockShared(&subscriber->m_lock);
		}
		else break;
	}
	return 0;
}




VOID CALLBACK MyWorkCallbackHandleClient(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
	// Instance, Parameter, and Work not used in this example.
	UNREFERENCED_PARAMETER(Instance);
	//	UNREFERENCED_PARAMETER(Parameter);
	UNREFERENCED_PARAMETER(Work);

	CServerTCPThreadpool *serverTCPThreadpool = (CServerTCPThreadpool *)Parameter;
	TCHAR_ADDRESS ip_address_client;
	deque<pair<SOCKET, TCHAR_ADDRESS> > *dequeSocket = &serverTCPThreadpool->deqSocket;
	CCriticalSection criticalSection(serverTCPThreadpool->mCriticalSection);
	if (dequeSocket->empty()) return;

	SOCKET ClientSocket = dequeSocket->front().first;
	strncpy(ip_address_client, dequeSocket->front().second, sizeof(ip_address_client));
	dequeSocket->pop_front();	
	criticalSection.Leave();
	ip_address_client[sizeof(ip_address_client) - 1] = 0;

//	iResult = ReadBuff(ClientSocket, recvbuf, recvbuflen);
	TBuff buff_cmd;
	//int iResult = recv(ClientSocket, (char *)&cmd, sizeof(E_Comm_Subcript), 0);
	int iResult = buff_cmd.ReadBuff(ClientSocket);
	if (iResult < 1)	{
		_printf("recv failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return;
	}
	const char *mess_send = "ok";
	// Обработка строки-команды
	if (HandleCmd(buff_cmd, ip_address_client)) {
		// Ошибка 
		mess_send = "er";
	}
	iResult = send(ClientSocket, mess_send, 2, 0);
	if (iResult == SOCKET_ERROR) {
		_printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return;
	}

	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		_printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return;
	}

	closesocket(ClientSocket);
	//	Sleep(1000);
	return;
}

int main()
{
	InitializeCriticalSection(&gCriticalSection_CONSOLE);

	CServerTCPThreadpool serverTCPThreadpool;

	if (!serverTCPThreadpool.Init())
		serverTCPThreadpool.Run();
 

	DeleteCriticalSection(&gCriticalSection_CONSOLE);

	getchar();
	getchar();
	return 0;
}

