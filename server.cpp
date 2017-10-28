#include <iostream>

#include <set>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

int set_nonblock (int fd)	{
	int flags;
#if defined (O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

int main(int argc, char **argv)	{
	int master_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	std::set<int> slave_sockets;
	
	struct sockaddr_in sock_addr;
	sock_addr.sin_family = PF_INET;
	sock_addr.sin_port = htons(12345);
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(master_socket, (struct sockaddr*)&sock_addr,
		 sizeof(sock_addr));

	set_nonblock(master_socket);

	listen(master_socket, SOMAXCONN);

	while(1)	{

		fd_set set;
		FD_ZERO(&set);
		FD_SET(master_socket, &set);
		for (auto iter = slave_sockets.begin();
			 iter != slave_sockets.end(); iter++)	
			FD_SET(*iter, &set);

		int max = std::max(master_socket,
						   *std::max_element(slave_sockets.begin(),
											 slave_sockets.end()));

		select(max + 1, &set, NULL, NULL, NULL);

		for (auto iter = slave_sockets.begin();
			 iter != slave_sockets.end(); iter++)	{
			if (FD_ISSET(*iter, &set))	{
				static char buffer[1024];
				int recv_size = recv(*iter,
									 buffer, 1024, MSG_NOSIGNAL);
				if (recv_size == 0 && errno != EAGAIN)	{
					shutdown(*iter, SHUT_RDWR);
					close(*iter);
				}
				else if (recv_size != 0)
					send(*iter, buffer,
						 recv_size, MSG_NOSIGNAL);
			}
		}
		if (FD_ISSET(master_socket, &set))	{
			int slave_socket = accept(master_socket, 0, 0);
			set_nonblock(slave_socket);
			slave_sockets.insert(slave_socket);
		}
	}
	
	return 0;
}
