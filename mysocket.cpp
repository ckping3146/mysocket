// mysocket.cpp : 定义 DLL 应用程序的导出函数。
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mysocket.h"

#ifdef WIN32
#include "stdafx.h"
#pragma comment(lib, "ws2_32.lib")
#include <winsock.h>
#define socklen_t int

#define bzero(a,b) memset(a, 0, b)
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <strings.h>
#include <sys/epoll.h>

#define closesocket close
#endif

#include <iostream>
#include <thread> // C++11 多线程库

using namespace std;

#ifdef WIN32 
#define HandleError(msg) \
	do {std::cout << "Error exit:"<< msg << std::endl; \
	exit(EXIT_FAILURE); } while(0)
#else
#define HandleError(msg) \
	do {fprintf(stderr, "%s:%s, exit.\n", msg, strerror(errno)); \
		exit(EXIT_FAILURE);} while(0)
#endif

MySocket::MySocket()
{
	// 类实例化的时候，WIN平台需要先加载网络库
#ifdef WIN32
	WSADATA ws;
	WORD socketVersion = MAKEWORD(2,2);
	if (WSAStartup(socketVersion, &ws) != 0) {
		HandleError("Error WSAStartup");
	}
#endif
	sockfd = -1;
}

// 适用于客户端的构造函数: 指定地址参数
MySocket::MySocket(int sock, unsigned short port, char * ip) {
#ifdef WIN32
	WSADATA ws;
	WORD socketVersion = MAKEWORD(2, 2);
	if (WSAStartup(socketVersion, &ws) != 0) {
		HandleError("Error WSAStartup");
	}
#endif
	sockfd = sock; pt = port; strcpy(ipaddr, ip);
}

int MySocket::Socket() {
	sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建基于TCP的套接字
	return sockfd;
}

bool MySocket::Bind(const unsigned short port, const int backlog) {
	sockaddr_in sa;
	pt = port;
	strcpy(ipaddr, "0.0.0.0");
	memset(&sa, 0, sizeof(sa));
	sa.sin_addr.s_addr = inet_addr(ipaddr);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(pt);
	//  绑定本地端口
	if (sockfd < 0)
		Socket();
	
	if (-1 == ::bind(sockfd, (sockaddr*)&sa, sizeof(sockaddr))) {
		std::cout << "Bind Port " << port << " Failed." << std::endl;
		return false;
	}
	if (-1 == listen(sockfd, backlog)) {
		std::cout << "Listen Port " << port << " Failed." << std::endl;
		return false;
	}
	std::cout << "Bind&Listen Port " << port << " Success." << std::endl;
	return true;
}

/* 默认的阻塞的ACCEPT */
MySocket MySocket::Accept() {
	sockaddr_in sa_cli;
	memset(&sa_cli, 0, sizeof(sa_cli));
	socklen_t len = sizeof(sa_cli);
	int cli_sock;
	if (-1 == (cli_sock = accept(sockfd, (sockaddr*)&sa_cli, &len))) {
		std::cout << "Accept error:" << strerror(errno) << std::endl;
		return MySocket(); // 失败返回一个空的实例（sockfd=-1）
	}
	MySocket client(cli_sock, ntohs(sa_cli.sin_port), inet_ntoa(sa_cli.sin_addr));
	std::cout << "Accepted Success From:[" << client.ip() << "][" << client.port() << "]" << std::endl;

	return client;
}

int MySocket::Send(const char * buf, const int size) {
	int sendedSize = 0;
	while (sendedSize < size) {
		int tmpSize = send(sockfd, buf + sendedSize, size - sendedSize, 0);
		if (-1 == tmpSize) break;
		sendedSize += tmpSize;
	}
	return sendedSize;
}

int MySocket::Recv(char * buf, int size) {
	return recv(sockfd, buf, size, 0);
}

/* 客户端调用 */
bool MySocket::Connect(const char * ip, const unsigned short port) {
	sockaddr_in dstaddr;
	memset(&dstaddr, 0, sizeof(dstaddr));
	if (0 >= sockfd)
		Socket();
	dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = inet_addr(ip);
	dstaddr.sin_port = htons(port);
	if (0 != connect(sockfd, (sockaddr*)&dstaddr, sizeof(dstaddr))) {
		std::cout << "Connecting [" << ip << "][" << port << "] Failed. " << std::endl;
		return false;
	}
	std::cout << "Connected[" << ip << "][" << port << "]." << std::endl;
	return true;
}

/* 客户端调用: 非阻塞版本 */
bool MySocket::ConnectNonBlock(const char * ip, const unsigned short port, const int tmo) {
	sockaddr_in dstaddr;
	memset(&dstaddr, 0, sizeof(dstaddr));
	if (0 >= sockfd)
		Socket();
	dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = inet_addr(ip);
	dstaddr.sin_port = htons(port);

	fd_set fds; // 用于监视 sockfd 的可写性变化 write
	bzero(&fds, sizeof(fd_set));
	FD_SET(sockfd, &fds);

	SetBlock(false); // 设置socket 为非阻塞
	if (0 != connect(sockfd, (sockaddr*)&dstaddr, sizeof(dstaddr))) {
		// socket 为非阻塞时， accept 会立即返回
		timeval tmv = {
			0, tmo * 1000
		};
		int ret = select(sockfd + 1, 0, &fds, 0, &tmv); // 监视这个套接字是否可写，阻塞知道tmv超时
		SetBlock(false); // 设置socket阻塞（default） 
		switch (ret) {
		case -1:
		case 0:
			if (-1==ret)
				std::cout << "ConnectNonBlock Error:" << strerror(errno) << std::endl;
			else
				std::cout << "ConnectNonBlock Timeout:" << strerror(errno) << std::endl;
			std::cout << "Connecting [" << ip << "][" << port << "] Failed. " << std::endl;
			return false; // 失败返回一个空的实例（sockfd=-1）
		default: // >0: 返回fdset 中就绪描述字的正数目
			break;
		}
	}
	SetBlock(false); // 设置socket阻塞（default） 
	std::cout << "Connect to [" << ip << "][" << port << "] Success." << std::endl;
	return true;
}

/* 设置 套接字 阻塞状态 */
bool MySocket::SetBlock(bool block) {
	int ret = -1;
#ifdef WIN32
	unsigned long nbio = (unsigned long)block;
	ret = ioctlsocket(sockfd, FIONBIO, &nbio);
#else /* Linux */
	int sockFlags = fcntl(sockfd, F_GETFL);
	if (block) sockFlags = sockFlags & ~O_NONBLOCK;
	else sockFlags = sockFlags | O_NONBLOCK;
	ret = fcntl(sockfd, F_SETFL, &sockFlags);
#endif  // WIN32
	if (-1 == ret ) {
		std::cout << "SetBlock on " << socket << " failed :"<< strerror(errno) << std::endl;
		return false;
	}
	return true;
}

void MySocket::Close() {
	if(sockfd > 0)
		closesocket(sockfd);
	sockfd = -1;
}

MySocket::~MySocket()
{
}

// 打印socket信息
void MySocket::Show() {
	std::cout << "\tIP\t:" << ipaddr << std::endl;
	std::cout << "\tPort\t:" << pt << std::endl;
	std::cout << "\tFd\t:" << sockfd << std::endl;
}

// 测试
// 1） 测试epoll多并发
#ifndef WIN32 /* epoll 只能在linux下 */
void MySocket::StartTestEpollServer(int argc, char ** argv) {
	uint16_t port = 8080;
	if (argc > 1)
		port = atoi(argv[1]);

	/* 在内核创建 epoll 实例 */
	int epfd = epoll_create(200); // 可以监听的描述符个数，但是man文档是说只要是个正数就可以，size is ignored
	if (-1 == epfd) {
		cout << "Create epoll Failed : " << strerror(errno) << endl;
		return;
	}

	MySocket server;
	if (!server.Bind(port)) {
		server.Close(); return;
	}

	epoll_event epev, epevs[20];
	bzero(&epev, sizeof(epev));
	bzero(epevs, 20 * sizeof(epoll_event));

	// 设置要处理得时间
	epev.data.fd = server.sock();	// 要处理事件得相关文件描述符
	epev.events = EPOLLIN | EPOLLET;// 要处理得事件类型

	// 注册要处理得事件
	epoll_ctl(epfd, EPOLL_CTL_ADD, server.sock(), &epev);

	for (;;) {
		//MySocket client = server.Accept();
		//if (client.sock() < 0) break;

		// wait for epoll event occur
		int nfds /* 同时发生的事件数 */ 
			= epoll_wait(epfd, epevs, 20, 500);
		for (int i = 0; i < nfds; ++i) {
			// 有新连接到来
			if (epevs[i].data.fd == server.sock()) {
				;
			}
			// 
			else if {
				;
			}
			else {
				;
			}
		}
	}

	server.Close();
}
#endif
