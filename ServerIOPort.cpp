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
#include "server_data.h"
#pragma comment(lib, "libpq.lib")
#pragma comment(lib, "Ws2_32.lib")
PGconn * db;
bool start_db() {
	char request[] = "dbname = postgres user = postgres password = PaSsWoRd \
    hostaddr = 127.0.0.1 port = 1025";
	db = PQconnectdb(request);
	if (PQstatus(db) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n",
			PQerrorMessage(db));
		return 0;
	}
	return 1;
}
int main()
{
	HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	if (!start_db()) return 1;
	server_data server(db, iocp);
	while(true){}
	return 0;
}
