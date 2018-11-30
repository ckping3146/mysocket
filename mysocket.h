#ifndef MYSOCKET_H
#define MYSOCKET_H

#ifdef WIN32
// 导出到库
#ifdef MYSOCKET_EXPORTS
#define MYSOCKET_API __declspec(dllexport)
#else
#define MYSOCKET_API __declspec(dllimport)
#endif // MYSOCKET_EXPORT

#else
#define MYSOCKET_API 
#endif // WIN32

class MYSOCKET_API MySocket
{
public:
	MySocket();
	// 适用于客户端的构造函数
	MySocket(int sock, unsigned short port, char * ip);
	virtual ~MySocket();
	// 读接口
	const int sock() const { return sockfd; }  ;
	const unsigned short port() const { return pt; };
	const char * ip() const { return ipaddr; };

	// 封装Socket接口
	int Socket();
	bool Bind(const unsigned short port, const int backlog = 50);
	MySocket Accept();
	void Close();
	int Send(const char * buf, const int size);
	int Recv(char * buf, int size);
	bool Connect(const char * ip, const unsigned short port);

	// 设置阻塞/非阻塞接口
	bool SetBlock(bool block);

	// 非阻塞的 Socket 接口
	bool ConnectNonBlock(const char * ip, const unsigned short port, const int tmo = 1000);

	// 打印socket信息
	void Show();

	// 测试
	// 用于测试多并发epoll服务器
#ifndef WIN32 /* epoll 只能在linux下 */
	void StartTestEpollServer(int argc, char ** argv);
#endif
private:
	int sockfd;
	unsigned short pt;
	char ipaddr[16];
	//
};

#endif
