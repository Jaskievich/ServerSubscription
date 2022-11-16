// ServerSubscription.cpp : Defines the entry point for the console application.
//



#include "stdafx.h"

#include <time.h>
#include <process.h>

#include "ServerTCPThreadPool.h"




#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"
#define PORT_UDP 27017
#define N_SEND 10


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

	int Send(const TCHAR_ADDRESS _ip_address, long plant)
	{
		si_other.sin_addr.S_un.S_addr = inet_addr(_ip_address);
		int i = 0, slen = sizeof(si_other);
		for (; i < 3; ++i) {
			cod_err = 0;
			//send the message
		//	if (sendto(m_socket, "ok", 2, 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)	{
			if (sendto(m_socket, (char *)&plant, 2, 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR) {
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
	long plant;
	HANDLE m_hEventClose;
	HANDLE hThread;
	SRWLOCK m_lock;

	/*CSubscriber(const char *_name_event)
	{
		strncpy(name_event, _name_event, MAX_PATH - 1);
		name_event[MAX_PATH - 1] = 0;
		m_hEventClose = CreateEvent(NULL, TRUE, FALSE, NULL);
		InitializeSRWLock(&m_lock);
	}*/

	CSubscriber(long _plant)
	{
		plant = _plant;
		sprintf(name_event, "Global\\U%i", _plant);
		name_event[MAX_PATH - 1] = 0;
		m_hEventClose = CreateEvent(NULL, TRUE, FALSE, NULL);
		InitializeSRWLock(&m_lock);
	}

	/*
	void Add_Alias(const char *name_event) 
	{
		AcquireSRWLockExclusive(&m_lock);
		vName_alias_event.push_back(name_event);
		ReleaseSRWLockExclusive(&m_lock);
	}
	*/

	/*
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
	*/

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

	/*
	int GetVectorHandle(vector<HANDLE> &vHanle)
	{
		vHanle.clear();
		HANDLE hEventChange = OpenEvent(SYNCHRONIZE, TRUE, name_event);
		if (!hEventChange) {
			_printf("OpenEvent error: %d\n", GetLastError());
			return 1;
		}
		vHanle.push_back(hEventChange);
		for (int i = 0; i < vName_alias_event.size(); ++i) {
			HANDLE hCurrEvent = OpenEvent(SYNCHRONIZE, TRUE, vName_alias_event[i].c_str());
			if(hCurrEvent) vHanle.push_back(hCurrEvent);
		}
		vHanle.push_back(m_hEventClose);
		return 0;
	}
	*/
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

	/*inline int _IndexSubScriberByNameEvent(const char *name_event)
	{		
		for (int i = 0; i < vSubscriber.size(); ++i)
			if (strcmp(vSubscriber[i]->name_event, name_event) == 0) return i;
		return -1;
	}
*/
	inline int _IndexSubScriberByPlant(long plant)
	{
		for (int i = 0; i < vSubscriber.size(); ++i)
			if (vSubscriber[i]->plant == plant ) return i;
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

	/*CSubscriber * SubScriberByNameEvent(const char *name_event)
	{
		CSubscriber *res = NULL;
		AcquireSRWLockShared(&m_lock);
		int index = _IndexSubScriberByNameEvent(name_event);
		if( index > -1)	res = vSubscriber[index];
		ReleaseSRWLockShared(&m_lock);
		return res;
	}
*/
	CSubscriber * SubScriberByPlant(long plant)
	{
		CSubscriber *res = NULL;
		AcquireSRWLockShared(&m_lock);
		int index = _IndexSubScriberByPlant(plant);
		if (index > -1)	res = vSubscriber[index];
		ReleaseSRWLockShared(&m_lock);
		return res;
	}

	void AddSubscriber(CSubscriber *subscriber)
	{
		AcquireSRWLockExclusive(&m_lock);
		vSubscriber.push_back(subscriber);
		ReleaseSRWLockExclusive(&m_lock);
	}

	/*void DelSubscriber(const char *name_event)
	{
		AcquireSRWLockExclusive(&m_lock);
		int i = _IndexSubScriberByNameEvent(name_event);
		if (i > -1) {
			vector<CSubscriber *>::iterator it = vSubscriber.begin() + i;
			if (it < vSubscriber.end()) vSubscriber.erase(it);
		}
		ReleaseSRWLockExclusive(&m_lock);
	}*/

	void DelSubscriber(long plant)
	{
		AcquireSRWLockExclusive(&m_lock);
		int i = _IndexSubScriberByPlant(plant);
		if (i > -1) {
			vector<CSubscriber *>::iterator it = vSubscriber.begin() + i;
			if (it < vSubscriber.end()) vSubscriber.erase(it);
		}
		ReleaseSRWLockExclusive(&m_lock);
	}
};



CListSubscriber g_ListSubscriber;  // гдобальнай список


enum E_Comm_Subcript
{
	E_ADD,
	E_DEL
	//E_ADD_ALIAS,
	//E_DEL_ALIAS
};

// Обработка комманд
E_Result HandleCmd(TSocketData &buff_cmd, TCHAR_ADDRESS address )
{
	long plant = (long )buff_cmd.header.param;
	if(!plant)return e_Err;
	CSubscriber *subs = g_ListSubscriber.SubScriberByPlant(plant);
	switch (buff_cmd.header.cmd)
	{
	case E_ADD: {		
		if (subs == NULL) {
			subs = new CSubscriber(plant);
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
				g_ListSubscriber.DelSubscriber(subs->plant);
			}
			else subs->DelIpAdress(address);
		}
		break;
/*
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
	*/
	}
	return e_OK;
}


//*
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
				if ( UDPClient.Send(subscriber->vIP_Address[i].c_str(), subscriber->plant) ) {
					ReleaseSRWLockShared(&subscriber->m_lock);
					// удалить Ip-adress
					subscriber->DelIpAdress(subscriber->vIP_Address[i].c_str());
					AcquireSRWLockShared(&subscriber->m_lock);
					if (subscriber->vIP_Address.size() == 0) {// Удалить из списка подписчиков
						g_ListSubscriber.DelSubscriber(subscriber->plant);
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

//*/

/* // Обработчик потока
unsigned __stdcall Handle_event_thread(LPVOID lpvParam)
{
	CSubscriber *subscriber = (CSubscriber *)lpvParam;
	vector<HANDLE> arr_handele;
	if (subscriber->GetVectorHandle(arr_handele)) return 0;
	CUDPClient UDPClient;
	if (UDPClient.Init()) return 0;
	DWORD iRes = 0;
	int size = arr_handele.size();
	while (true) {
		iRes = WaitForMultipleObjects(size, &arr_handele[0], FALSE, INFINITE);
		if (iRes >= WAIT_OBJECT_0 && iRes < WAIT_OBJECT_0 + size) {
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
*/


int main()
{
	InitializeCriticalSection(&gCriticalSection_CONSOLE);

	CServerTCPThreadpool serverTCPThreadpool(DEFAULT_PORT);

	if (!serverTCPThreadpool.Init())
		serverTCPThreadpool.Run();
 

	DeleteCriticalSection(&gCriticalSection_CONSOLE);

	getchar();
	getchar();
	return 0;
}

