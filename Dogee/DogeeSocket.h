#ifndef __DOGEE_SOCKET_H_
#define __DOGEE_SOCKET_H_

#ifdef _WIN32
#include <winsock.h>
#define RcSocketLastError() WSAGetLastError()
#else
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
typedef struct sockaddr* LPSOCKADDR;
#define RcSocketLastError() errno
#endif
namespace Dogee
{
	namespace Socket
	{
		extern SOCKET RcCreateListen(int port);
		extern void RcSetTCPNoDelay(SOCKET fd);
		extern SOCKET RcConnect(char* ip, int port);
		extern SOCKET RcListen(int port);
		extern SOCKET RcAccept(SOCKET slisten);

		inline int RcSend(SOCKET s, void* data, size_t len)
		{
			return send((SOCKET)s, (char*)data, len, 0);
		}

		inline int RcRecv(SOCKET s, void* data, size_t len)
		{
			return recv((SOCKET)s, (char*)data, len, 0);
		}

		inline int RcCloseSocket(SOCKET s)
		{
			return closesocket((SOCKET)s);
		}
		extern void get_peer_ip_port(SOCKET fd, std::string& ip, int& port);
		extern SOCKET RcTryConnect(char* ip, int port, int node_id);
	}
}

#endif