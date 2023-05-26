#pragma comment (lib, "ws2_32.lib")

#include "WSAInitializer.h"
#include "Server.h"
#include <iostream>
# include <thread>
#include <exception>
# include <set>
#include <string>

# define PORT 8826

int main()
{
	try
	{
		WSAInitializer wsaInit;
		std::set<std::string> users;
		Server* server = new Server();

		std::thread acceptTh([server] { server->serve(PORT); });
		acceptTh.detach();

		std::thread readThread([server] { server->readMessages(); });

		readThread.join();

	}
	catch (std::exception& e)
	{
		std::cout << "Error occured: " << e.what() << std::endl;
	}
	system("PAUSE");
	return 0;
}

