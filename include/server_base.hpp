/*
 * author: jokhan
 * date: 2016.11.16
 * function: server_base.hpp
 */
#ifndef SERVER_BASE_HPP
#define SERVER_BASE_HPP

#include <unordered_map>
#include <thread>
#include <string>
#include <regex>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <map>
#include <fstream>


namespace Myweb {

	struct Request {
		//method: POST or GET;
		std::string method, path, http_version;
		//用智能指针对content的引用计数
		std::shared_ptr<std::istream> content;
		//key-value
		std::unordered_map<std::string, std::string> header;
		// 用正则表达式处理路径匹配
		std::smatch path_match;
	};

	typedef std::map<std::string, std::unordered_map<std::string,std::function<void(std::ostream&, Request&) >>> resource_type;

	// socket_type: HTTP or HTTPS
	template <typename socket_type>
	class ServerBase {
	public:
		// 构造服务器，初始化端口，默认使用一个线程
		ServerBase(unsigned short port, size_t num_threads=1) : endpoint(boost::asio::ip::tcp::v4(), port), acceptor(m_io_service, endpoint), num_threads(num_threads) {}
		//start the web server
		void start() {
			for( auto it=resource.begin(); it!=resource.end(); it++ ) {
				all_resources.push_back(it);
			}
			// 默认的请求放在放在vector尾部
			for( auto it=default_resource.begin(); it!=default_resource.end(); it++ ) {
				all_resources.push_back(it);
			}
			// 调用socket的连接方式，还需要子类来实现accept()逻辑
			accept();

			// 如何num_threads > 1，那么m_io_service.run()
			// 将运行(num_threads-1)线程成为线程池
			for( size_t i = 1; i < num_threads; i ++ ) {
				threads.emplace_back([this](){
						m_io_service.run();
						});
			}

			// 主线程
			m_io_service.run();

			// 等待其他线程，如果有的话，就等待这些线程的结束
			for(auto& t: threads) {
				t.join();
			}
		}
		// 用于服务器访问资源处理方式
		resource_type resource;
		// 用于保存默认资源的处理方式
		resource_type default_resource;
	protected:
		// 用于内部实现对所有资源的处理 
		std::vector<resource_type::iterator> all_resources;
		// 不同的服务器实现方法不同
		virtual void accept() {}
		// 处理请求和应答
		void process_request_and_respond(std::shared_ptr<socket_type> socket) const {
			// 为async_read_untile()创建新的读缓存
			// shared_ptr 用于传递临时对象给匿名函数
			// 会被推导为std::shared_ptr<boost::asio::streambuf>
			auto read_buffer = std::make_shared<boost::asio::streambuf>();

			boost::asio::async_read_until(*socket, *read_buffer, "\r\n\r\n", [this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
					if (!ec) {
					// 注意：read_buffer->size()的大小并一定和bytes_transferred相等，boost的文档中指出
					// 在async_read_until操作成功后，streambuf在界定符之外可能包含一些额外的数据
					// 所以较好的做法是直接从流中提取并解析当前read_buffer左边的报头，再拼接async_read后面的内容
					size_t total = read_buffer->size();

					// 转换到istream
					std::istream stream(read_buffer.get());

					// 被推导为std::shared_ptr<Request>类型
					auto request = std::make_shared<Request>();

					// 接下来将stream中的请求信息进行解析，然后保存到request对象中
					*request = parse_request(stream);

					size_t num_additional_bytes = total - bytes_transferred;

					// 如果满足，同样读取
					if(request->header.count("Content-Length")>0) {
					boost::asio::async_read(*socket, *read_buffer,
						boost::asio::transfer_exactly(stoull(request->header["Content-Length"]) - num_additional_bytes),
						[this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes_transferred) {
						if(!ec) {
						// 将指针作为istream对象存储到read_buffer中
						request->content = std::shared_ptr<std::istream>(new std::istream(read_buffer.get()));
						respond(socket, request);
						}
						}
						);
					} else {
						respond(socket, request);
					}
					}
					});
		}

		// 响应
		void respond(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) const {
			//对请求路径和方法进行匹配查找，并生成响应
			for(auto res_it : all_resources) {
				std::regex e(res_it->first);
				std::smatch sm_res;
				if(std::regex_match(request->path, sm_res, e)) {
					if(res_it->second.count(request->method) > 0) {
						request->path_match = move(sm_res);

						// 会被推导为std::shared_ptr<boost::asio::streambuf>
						auto write_buffer = std::make_shared<boost::asio::streambuf>();
						std::ostream response(write_buffer.get());
						res_it->second[request->method](response, *request);

						// 在lambda 中捕获write_buffer使其不会在async_write完成前被销毁
						boost::asio::async_write(*socket, *write_buffer,
								[this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {

								// HTTP持久连接(HTTP 1.1 ), 递归调用
								if( !ec && stof(request->http_version) > 1.05 ) {
								process_request_and_respond(socket);
								}
								}
								);
						return;
					}
				}
			}
		}

		// 解析请求
		Request parse_request(std::istream& stream) const {
			Request request;

			// 使用正则表达式对请求报头进行解析，通过下面的正则表达式
			// 可以解析出请求方法(GET/POST)、请求路径以及HTTP版本
			std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

			std::smatch sub_match;

			// 从第一行中解析请求方法、路径和HTTP版本
			std::string line;
			getline(stream, line);
			line.pop_back();
			if(std::regex_match(line, sub_match, e)) {
				request.method = sub_match[1];
				request.path = sub_match[2];
				request.http_version = sub_match[3];

				// 解析头部的其他信息
				bool matched;
				e="^([^:]*): ?(.*)$";
				do {
					getline(stream,line);
					line.pop_back();
					matched=std::regex_match(line, sub_match, e);
					if (matched) {
						request.header[sub_match[1]] = sub_match[2];
					}
				} while (matched==true);
			}
			return request;
		}

		boost::asio::io_service m_io_service;
		boost::asio::ip::tcp::endpoint endpoint;
		boost::asio::ip::tcp::acceptor acceptor;

		size_t num_threads;
		std::vector<std::thread> threads;
	};
	template <typename socket_type>
	class Server : public ServerBase<socket_type> {};

}

#endif
