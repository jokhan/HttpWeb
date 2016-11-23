/*
 * author: jokhan
 * date: 2016.11.17
 * function: main_http.cpp
 */
#include "./include/server_http.hpp"
#include "./include/handler.hpp"

using namespace Myweb;

int main()
{
	// HTTP 服务运行在12345端口，并启用四个线程
	Server<HTTP> server(12345,4);
	start_server<Server<HTTP> >(server);
	return 0;
}
