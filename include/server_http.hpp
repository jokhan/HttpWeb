/*
 * author: jokhan
 * date: 2016.11.17
 * function: server_http.hpp
 */
#ifndef SERVER_HTTP_HPP
#define SERVER_HTTP_HPP

#include "server_base.hpp"

namespace Myweb {
	typedef boost::asio::ip::tcp::socket HTTP;
	template<>
	class Server<HTTP> : public ServerBase<HTTP> {
	public:
		// 通过端口号，线程数来构造web服务器，HTTP服务器比较简单，不需要相关配置文件的初始化
		Server(unsigned short port, size_t num_threads=1) : ServerBase<HTTP>::ServerBase(port, num_threads) {};
	private:
		// 实现accept()方法
		void accept() {
			// 为当前连接创建一个新的socket
			// shared_ptr用于传递临时对象给匿名函数
			// socket会被推到为std::shared_ptr<HTTP>类型
			auto socket = std::make_shared<HTTP>(m_io_service);

			acceptor.async_accept(*socket,
					[this, socket](const boost::system::error_code& ec) {
					// 立即启动并接受一个连接
					accept();
					if(!ec) {
					process_request_and_respond(socket);
					}
					}
					);
		}
	};
}


#endif
