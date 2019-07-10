#include "pch.h"
#include "server_data.h"
#include <windows.system.diagnostics.h>

server_data::server_data(PGconn * db,HANDLE iocp):db(db),iocp(iocp),t(&server_data::NewClient,this)
{
	for (int i = 0; i < 2; i++) {
		std::thread * work_t = new std::thread(&server_data::static_proxy, this);
		threads.push_back(work_t);
	}
}
struct server_data::Sockets {
	SOCKET s;
	WSAOVERLAPPED ov_recv;
	WSAOVERLAPPED ov_send;
	char buf[4096];
	int index;
	int client_id;
	DWORD flag = 0;
};

server_data::~server_data()
{
	server_conn = 0;
	closesocket(this->arr[0]->s);
	t.detach();
	for (std::thread * x : threads) {
		x->detach();
		delete x;
	}
	for (Sockets * x : arr) {
		delete x;
	}
	WSACleanup();
}

void server_data::NewClient() {
	Start_Server();
	SOCKADDR_IN guest_addr;
	guest_addr.sin_family = AF_INET;
	int size = sizeof(guest_addr);
	arr[0]->ov_recv.hEvent = WSACreateEvent();
	WSAEventSelect(arr[0]->s, arr[0]->ov_recv.hEvent, FD_ACCEPT);
	while (server_conn) {
		WSANETWORKEVENTS a_event;
		WSAEnumNetworkEvents(arr[0]->s, arr[0]->ov_recv.hEvent, &a_event);
		SOCKET guest;
		if ((a_event.lNetworkEvents & FD_ACCEPT) != 0) {
			ResetEvent(arr[0]->ov_recv.hEvent);
			guest = WSAAccept(arr[0]->s, (sockaddr*)&guest_addr, &size, 0, 0);
			HOSTENT *hst;
			hst = gethostbyaddr((char *)
				&guest_addr.sin_addr.s_addr, 4, AF_INET);
			printf("+%s [%s] new connect!\n",
				(hst) ? hst->h_name : "",
				inet_ntoa(guest_addr.sin_addr));
			char reg[1024];
			memset(reg, 0, sizeof(reg));
			recv(guest, reg, sizeof(reg), 0);
			std::string str = reg;
			if (str.substr(0, 1) == "l") {
				std::string login, password;
				password = str.substr(1, (str.find(" ") - 1));
				login = str.substr(str.find(" ") + 1, (str.length() - str.find(" ")));
				logining(login, password, this->db, this->iocp, guest);
			}
			else if (str.substr(0, 1) == "r") {
				std::string login, password;
				password = str.substr(1, (str.find(" ") - 1));
				login = str.substr(str.find(" ") + 1, (str.length() - str.find(" ")));
				Registration(login, password, this->db, guest);
			}
		}
	}
}
void server_data::logining(std::string login, std::string password, PGconn * conn, HANDLE iocp, SOCKET sock) {
	std::string str;
	str = "select exists(select * from users where password = '" + password + "' and login = '" + login + "');";
	char array[1024];
	strcpy_s(array, str.c_str());
	PGresult * res = PQexec(conn, array);
	Sockets * guest = new Sockets;
	if (*PQgetvalue(res, 0, 0) == 't') {
		std::string str1 = "select id from users where password = '" + password + "' and login = '" + login + "';";
		char array1[1024];
		strcpy_s(array1, str1.c_str());
		PGresult * res = PQexec(conn, array1);
		std::string forconvert = PQgetvalue(res, 0, 0);
		int id = stoi(forconvert);
		if (!freesockets.empty()) {
			int id_s = freesockets[freesockets.size() - 1];
			freesockets.pop_back();
			arr[id_s] = guest;
			guest->s = sock;
			guest->client_id = id;
			guest->index = id_s;
			memset(guest->buf, 0, sizeof(guest->buf));
			guest->ov_recv.hEvent = WSACreateEvent();
			guest->ov_send.hEvent = WSACreateEvent();
			reinterpret_cast<int&>(guest->ov_send.hEvent) |= 0x1;
			WSABUF buf;
			buf.buf = guest->buf;
			buf.len = sizeof(guest->buf);
			DWORD flag = 0;
			CreateIoCompletionPort((HANDLE)guest->s, iocp, guest->index, 0);
			memset(guest->buf, ' ', 1);
			WSARecv(guest->s, &buf, 1, 0, &flag, &guest->ov_recv, 0);
			str1 = "t" + forconvert;
			char tosend[20];
			strcpy_s(tosend, str1.c_str());
			WSABUF buffer;
			buffer.len = strlen(tosend);
			buffer.buf = tosend;			
			WSASend(guest->s, &buffer, 1, 0, 0, &guest->ov_send, 0);

		}
		else
		{
			arr.push_back(guest);
			guest->index = (arr.size() - 1);
			guest->s = sock;
			guest->client_id = id;
			memset(arr[guest->index]->buf, 0, sizeof(arr[guest->index]->buf));
			guest->ov_recv.hEvent = WSACreateEvent();
			guest->ov_send.hEvent = WSACreateEvent();
			reinterpret_cast<int&>(guest->ov_send.hEvent) |= 0x1;
			CreateIoCompletionPort((HANDLE)guest->s, iocp, guest->index, 0);
			WSABUF buf;
			buf.buf = arr[guest->index]->buf;
			buf.len = sizeof(arr[guest->index]->buf);
			DWORD flag = 0;
			memset(guest->buf, ' ', 1);
			WSARecv(guest->s, &buf, 1, 0, &flag, &guest->ov_recv, 0);
			str1 = "t" + forconvert;
			char tosend[20];
			strcpy_s(tosend, str1.c_str());
			WSABUF buffer;
			buffer.len = strlen(tosend);
			buffer.buf = tosend;
			WSASend(arr[guest->index]->s, &buffer, 1, 0, 0, &arr[guest->index]->ov_send, 0);
		}

		PQclear(res);
	}
	else
	{
		char tosend[] = "f";
		WSABUF buffer;
		buffer.len = strlen(tosend);
		buffer.buf = tosend;
		WSAOVERLAPPED over;
		over.hEvent = WSACreateEvent();
		WSASend(sock, &buffer, 1, 0, 0, &over, 0);
	}

}
void server_data::Registration(std::string login, std::string password, PGconn * conn, SOCKET s) {

	if (!check_space(login) && !check_space(password)) {
		std::string str;
		str = "select exists(select * from users where login = '" + login + "');";
		char array[1024];
		strcpy_s(array, str.c_str());
		PGresult * res = PQexec(conn, array);
		if (*PQgetvalue(res, 0, 0) == 'f') {
			std::string str = "insert into users (login,password) values ('" + login + "','" + password + "');";
			memset(array, 0, sizeof(array));
			strcpy_s(array, str.c_str());
			PGresult * res = PQexec(conn, array);
			char tosend[] = "t";
			WSABUF buffer;
			buffer.len = strlen(tosend);
			buffer.buf = tosend;
			WSAOVERLAPPED over;
			over.hEvent = WSACreateEvent();
			WSASend(s, &buffer, 1, 0, 0, &over, 0);
		}
		else
		{
			char tosend[] = "ue";
			WSABUF buffer;
			buffer.len = strlen(tosend);
			buffer.buf = tosend;
			WSAOVERLAPPED over;
			over.hEvent = WSACreateEvent();
			WSASend(s, &buffer, 1, 0, 0, &over, 0);
		}
	}
	else
	{
		char tosend[] = "f";
		WSABUF buffer;
		buffer.len = strlen(tosend);
		buffer.buf = tosend;
		WSAOVERLAPPED over;
		over.hEvent = WSACreateEvent();
		WSASend(s, &buffer, 1, 0, 0, &over, 0);
	}
}
bool server_data::check_space(std::string str) {
	char array[4096];
	strcpy_s(array, str.c_str());
	for (int x = 0; x < strlen(array); x++) {
		if (array[x] == ' ') {
			return 1;
		}
	}
	return 0;
}
DWORD server_data::CheckData()
{
	DWORD bytes;
	ULONG_PTR key = 0;
	WSAOVERLAPPED * overlapped;
	while (server_conn) {
		GetQueuedCompletionStatus(this->iocp, &bytes, &key, &overlapped, INFINITE);
		if (GetLastError() == 64 || arr[key]->buf[0] == ' ') {
			stop_conn(key);
			SetLastError(0);
		}
		else
		{
			process_data(key);
			cont_recv(key);
		}
	}
	return 0;
}
void server_data::process_data(int id) {
	std::string str = arr[id]->buf;
	strcpy_s(arr[id]->buf, str.c_str());
	memset(arr[id]->buf, 0, sizeof(arr[id]->buf));
	memset(arr[id]->buf, ' ', 1);
	if (str.substr(0, 2) == "sc") {
		str.erase(0, 2);
		std::string str2 = "SELECT EXISTS(SELECT * FROM users WHERE login = '" + str + "');";
		char array[1024];
		strcpy_s(array, str2.c_str());
		PGresult * res = PQexec(db, array);
		if (*PQgetvalue(res, 0, 0) == 't') {
			str2 = "SELECT id FROM users WHERE login = '" + str + "';";
			memset(array, 0, sizeof(array));
			strcpy_s(array, str2.c_str());
			PGresult * res = PQexec(db, array);
			str2 = "st";
			str2 += PQgetvalue(res, 0, 0);
			strcpy_s(array, str2.c_str());
			WSABUF buffer;
			buffer.len = strlen(array);
			buffer.buf = array;
			WSASend(arr[id]->s, &buffer, 1, 0, 0, &arr[id]->ov_send, 0);
		}
		else if (*PQgetvalue(res, 0, 0) == 'f') {
			memset(array, 0, sizeof(array));
			array[0] = 's';
			array[1] = 'f';
			WSABUF buffer;
			buffer.len = strlen(array);
			buffer.buf = array;
			WSASend(arr[id]->s, &buffer, 1, 0, 0, &arr[id]->ov_send, 0);
		}
	}
	else if (str.substr(0, 2) == "um") {
		str.erase(0, 2);
		int id1 = stoi(str.substr(0, str.find('/')));
		int user_to_id = id1;
		str.erase(0, (str.find('/') + 1));
		char buf[4096];
		if (ifonline(&id1)) {
			std::string str1 = "SELECT login FROM USERS WHERE id = " + std::to_string(arr[id]->client_id) + ";";
			memset(buf, 0, sizeof(buf));
			strcpy_s(buf, str1.c_str());
			PGresult * res = PQexec(db, buf);
			std::string str_tosend = "um" + std::to_string(arr[id]->client_id) + "/" + PQgetvalue(res, 0, 0) + "/" + str;
			strcpy_s(buf, str_tosend.c_str());
			WSABUF buffer;
			buffer.len = strlen(buf);
			buffer.buf = buf;
			WSASend(arr[id1]->s, &buffer, 1, 0, 0, &arr[id1]->ov_send, 0);
			str = "INSERT INTO messages(mess_from,mess_to,was_recv,mess_text) VALUES ('" + std::to_string(arr[id]->client_id) + " ','" + std::to_string(user_to_id) + "','1','" + str + "');";
			memset(buf, 0, sizeof(buf));
			strcpy_s(buf, str.c_str());
			PQexec(db, buf);
		}
		else
		{
			str = "INSERT INTO messages(mess_from,mess_to,was_recv,mess_text) VALUES ('" + std::to_string(arr[id]->client_id) + " ',' " + std::to_string(user_to_id) + "','0','" + str + "');";
			memset(buf, 0, sizeof(buf));
			strcpy_s(buf, str.c_str());
			PQexec(db, buf);
		}
	}
	else if (str.substr(0, 2) == "sm") {
		str.erase(0, 2);
		std::string to_query;
		if (str != "0") {
			to_query = "select count(*) from messages where time > '" + str + "' and mess_to = " + std::to_string(arr[id]->client_id) + ";";
		}
		else
		{
			to_query = "select count(*) from messages where time > '0001-01-01 00:00:00' and mess_to = " + std::to_string(arr[id]->client_id) + "or mess_from = " + std::to_string(arr[id]->client_id) + ";"; 
		}
		char buf[1000];
		memset(buf, 0, sizeof(buf));
		strcpy_s(buf, to_query.c_str());
		PGresult * res = PQexec(db, buf);
		std::string count_str = PQgetvalue(res, 0, 0);
		int count = std::stoi(count_str);
		if (str != "0") {
			to_query = "select login,mess_from,time,mess_text from messages inner join users on messages.mess_from = users.id where time > '" + str + "' and mess_to =" + std::to_string(arr[id]->client_id) + " order by time;";
			std::cout << to_query << std::endl;
			memset(buf, 0, sizeof(buf));
			strcpy_s(buf, to_query.c_str());
			res = PQexec(db, buf);
			for (int i = 0; i < count; i++) {
				std::string to_send;
				to_send += "sm";
				for (int q = 0; q < 4; q++) {
					if (q == 0) {
						to_send += PQgetvalue(res, i, q);
						to_send += " ";
					}
					else
					{
						to_send += PQgetvalue(res, i, q);
						if (q != 3)
							to_send += "/";
					}
				}
				char send_arr[5100];
				memset(send_arr, 0, sizeof(send_arr));
				strcpy_s(send_arr, to_send.c_str());
				WSABUF buffer;
				buffer.len = strlen(send_arr);
				buffer.buf = send_arr;
				WSASend(arr[id]->s, &buffer, 1, 0, 0, &arr[id]->ov_send, 0);
			}
		}
		else
		{
			to_query = "select login,mess_from,time,mess_text from messages inner join users on messages.mess_from = users.id where time > '0001-01-01 00:00:00' and mess_to =" + std::to_string(arr[id]->client_id) + " or mess_from = " + std::to_string(arr[id]->client_id) + " order by time;";
			std::cout << to_query << std::endl;
			memset(buf, 0, sizeof(buf));
			strcpy_s(buf, to_query.c_str());
			res = PQexec(db, buf);
			for (int i = 0; i < count; i++) {
				std::string to_send;
				to_send += "sm";
				for (int q = 0; q < 4; q++) {
					if (q == 0) {
						if (std::stoi(PQgetvalue(res, i, 1)) == arr[id]->client_id) {
							to_query = "select login,id from users inner join messages on users.id = messages.mess_to where mess_from =" + std::to_string(arr[id]->client_id)  +" and time = '";
							to_query += PQgetvalue(res, i, 2);
							to_query += "';";
							memset(buf, 0, sizeof(buf));
							strcpy_s(buf, to_query.c_str());
							PGresult * res1 = PQexec(db, buf);
							to_send += " ";
							to_send += PQgetvalue(res1, 0, 0);
							to_send += " ";
							to_send += PQgetvalue(res1, 0, 1);
							to_send += "/";
							to_send += PQgetvalue(res, i, 2);
							to_send += "/";
							to_send += PQgetvalue(res, i, 3);
							break;
						}
						to_send += PQgetvalue(res, i, q);
						to_send += " ";
					}
					else
					{
						to_send += PQgetvalue(res, i, q);
						if (q != 3)
							to_send += "/";
					}
				}
				char send_arr[5100];
				memset(send_arr, 0, sizeof(send_arr));
				strcpy_s(send_arr, to_send.c_str());
				WSABUF buffer;
				buffer.len = strlen(send_arr);
				buffer.buf = send_arr;
				WSASend(arr[id]->s, &buffer, 1, 0, 0, &arr[id]->ov_send, 0);
			}
		}
		
	}
}
void server_data::stop_conn(int id) {
	closesocket(arr[id]->s);
	freesockets.push_back(id);
	delete arr[id];
}
bool server_data::ifonline(int * id) {
	for (Sockets * x : arr) {
		if (x->client_id == *id) {
			*id = x->index;
			return 1;
		}
	}
	return 0;
}
void server_data::cont_recv(int id) {
	WSABUF buf;
	buf.buf = arr[id]->buf;
	buf.len = sizeof(arr[id]->buf);
	DWORD flag = 0;
	WSARecv(arr[id]->s, &buf, 1, 0, &flag, &arr[id]->ov_recv, 0);
}
void server_data::Start_Server() {
	int error;
	WSADATA ws;
	error = WSAStartup(MAKEWORD(2, 2), &ws);
	if (error) {
		printf("WSAStartup() failed with error: %d\n", error);
		return;
	}
	Sockets * main_sock = new Sockets;
	arr.push_back(main_sock);
	SOCKADDR_IN main_addr;
	main_addr.sin_family = AF_INET;
	main_addr.sin_addr.s_addr = INADDR_ANY;
	main_addr.sin_port = htons(80);
	arr[0]->s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (arr[0]->s == INVALID_SOCKET) {
		wprintf(L"socket function failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	error = ::bind(arr[0]->s, (sockaddr*)&main_addr, sizeof(main_addr));
	if (error == SOCKET_ERROR) {
		printf("bind() failed with error: %d\n", WSAGetLastError());
		closesocket(arr[0]->s);
		WSACleanup();
		return;
	}
	error = listen(arr[0]->s, SOMAXCONN);
	if (error == SOCKET_ERROR) {
		printf("listen() failed with error: %d\n", WSAGetLastError());
		closesocket(arr[0]->s);
		WSACleanup();
		return;
	}
}
DWORD __stdcall server_data::static_proxy(LPVOID obj)
{
	return static_cast<server_data*>(obj)->CheckData();
}
