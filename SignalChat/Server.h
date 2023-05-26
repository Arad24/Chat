#pragma once


# include <WinSock2.h>
# include <Windows.h>
# include "Helper.h"
# include <queue>
# include <map>
#include <mutex>          
#include <condition_variable>
# include <iostream>
# include <fstream>

# define SIZE 1024

struct Q_MSG
{
	std::string userMsg;
	SOCKET userSocket;
};

class Server
{
	public:
		Server();
		~Server();
		void serve(int port);
		void sendData(SOCKET cs, const std::string message);
		void readMessages();

	private:

		void acceptClient();
		void clientHandler(SOCKET clientSocket);
		int getMessageTypeCode(std::string msg);
		std::string getAllUsers();
		std::string findUsernameBySocket(SOCKET socket);

		boolean fileExist(std::string name);
		std::string getFileName(std::string firstName, std::string secName);

		SOCKET _serverSocket;
		std::queue<Q_MSG> _serverMessages;
		std::map<std::string, SOCKET> _onlineUsers;
};
