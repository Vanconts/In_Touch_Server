#pragma once
#include "pch.h"
#include <iostream>
#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#define _WIN32_WINNT 0x501
#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <thread>
#include <stdlib.h>
#include <libpq-fe.h>
#include <algorithm>
#pragma comment(lib, "libpq.lib")
#pragma comment(lib, "Ws2_32.lib")
class server_data
{
public:
	server_data(PGconn * db,HANDLE iocp);
	~server_data();
private:
	PGconn * db;
	HANDLE iocp;
	struct Sockets;
	void NewClient();
	void logining(std::string , std::string , PGconn * , HANDLE , SOCKET );
	void Registration(std::string , std::string , PGconn * , SOCKET );
	DWORD CheckData();
	bool check_space(std::string);
	void process_data(int);
	void stop_conn(int);
	bool ifonline(int *);
	void cont_recv(int);
	void Start_Server();
	static DWORD __stdcall static_proxy(LPVOID);
private:
	std::vector<int>freesockets;
	std::vector<Sockets*> arr;
	std::vector<std::thread*> threads;
	std::thread t;
	HANDLE h_t;
	bool server_conn = 1;
};

