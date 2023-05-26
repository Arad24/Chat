#include "Server.h"
#include <exception>
#include <iostream>
#include <string>
#include <thread>


Helper hp;

std::mutex m1;
std::condition_variable cond;



Server::Server()
{

	// this server use TCP. that why SOCK_STREAM & IPPROTO_TCP
	// if the server use UDP we will use: SOCK_DGRAM & IPPROTO_UDP
	_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (_serverSocket == INVALID_SOCKET)
		throw std::exception(__FUNCTION__ " - socket");
}

Server::~Server()
{
	try
	{
		// the only use of the destructor should be for freeing 
		// resources that was allocated in the constructor
		closesocket(_serverSocket);
	}
	catch (...) {}
}

void Server::serve(int port)
{

	struct sockaddr_in sa = { 0 };

	sa.sin_port = htons(port); // port that server will listen for
	sa.sin_family = AF_INET;   // must be AF_INET
	sa.sin_addr.s_addr = INADDR_ANY;    // when there are few ip's for the machine. We will use always "INADDR_ANY"

	// Connects between the socket and the configuration (port and etc..)
	if (bind(_serverSocket, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR)
		throw std::exception(__FUNCTION__ " - bind");

	// Start listening for incoming requests of clients
	if (listen(_serverSocket, SOMAXCONN) == SOCKET_ERROR)
		throw std::exception(__FUNCTION__ " - listen");
	std::cout << "listening..." << port << std::endl;

	while (true)
	{
		// the main thread is only accepting clients 
		// and add then to the list of handlers
		acceptClient();
	}
}


void Server::acceptClient()
{

	// this accepts the client and create a specific socket from server to this client
	// the process will not continue until a client connects to the server
	std::cout << "accepting client..." << std::endl;

	SOCKET client_socket = accept(_serverSocket, NULL, NULL);
	if (client_socket == INVALID_SOCKET)
		throw std::exception(__FUNCTION__);

	std::cout << "Client accepted!" << std::endl;
	// the function that handle the conversation with the client

	std::thread clientTh(&Server::clientHandler, this, client_socket);
	clientTh.detach();
	
}


void Server::clientHandler(SOCKET clientSocket)
{
	try
	{
		while (true)
		{
			// Get msg
			char m[SIZE] = { 0 };
			recv(clientSocket, m, SIZE - 1, 0);
			m[SIZE - 1] = 0;
			std::string msg = m;

			

			struct Q_MSG qMsg;
			qMsg.userMsg = msg;
			qMsg.userSocket = clientSocket;

			if (msg == "") throw std::exception("Socket closed.");

			std::unique_lock<std::mutex> locker(m1);

			this->_serverMessages.push(qMsg);

			locker.unlock();
			cond.notify_one();
		}
		

		//// Closing the socket (in the level of the TCP protocol)
		closesocket(clientSocket);
	}
	catch (const std::exception& e)
	{
		
		std::string username = findUsernameBySocket(clientSocket);
		std::string msg = "208" + username;
		struct Q_MSG qMsg;
		qMsg.userMsg = msg;
		qMsg.userSocket = clientSocket;

		std::unique_lock<std::mutex> locker(m1);
		this->_serverMessages.push(qMsg);

		locker.unlock();
		cond.notify_one();
	}

}

std::string Server::getAllUsers()
{
	std::string allUsers = "";
	std::map<std::string, SOCKET>::iterator itr;

	for (itr = this->_onlineUsers.begin(); itr != this->_onlineUsers.end(); itr++)
	{
		allUsers += itr->first + "&";
	}

	if (allUsers != "") allUsers.pop_back();

	return allUsers;
}

int Server::getMessageTypeCode(std::string msg)
{
	if (msg == "")
		return 0;

	int res = std::atoi(msg.c_str());
	return res;
}

void Server::sendData(SOCKET cs, const std::string message)
{
	const char* data = message.c_str();

	if (send(cs, data, message.size(), 0) == INVALID_SOCKET)
	{
		throw std::exception("Error while sending message to client");
	}
}



void Server::readMessages()
{
	std::string msgContent = "";

	while (true)
	{
		std::string username = "", secName = "", currMsg = "";
		int secondUserLen = 0;
		SOCKET clientSocket;


		try
		{
			if (!this->_serverMessages.empty())
			{
				// Get username & socket
				std::string msg = this->_serverMessages.front().userMsg;
				clientSocket = this->_serverMessages.front().userSocket;

				username = findUsernameBySocket(clientSocket);

				// Send message
				if (getMessageTypeCode(msg.substr(0, 3)) == MT_CLIENT_LOG_IN)
				{
					username = msg.substr(5, msg.length());

					this->_onlineUsers.insert({ username, clientSocket });

					// update from file
					std::string users = getAllUsers(), delimiter = "&";
					size_t pos = 0;
					while (( pos = users.find(delimiter)) != std::string::npos) {
						std::string fileName = getFileName(users.substr(0, pos), username);
						
						if (fileExist(fileName))
						{
							std::string fileTxt = "";
							std::ifstream file(fileName);
							std::getline(file, fileTxt);
							msgContent += fileTxt;
						}

						users.erase(0, pos + delimiter.length());
					}

				}
				else if (getMessageTypeCode(msg.substr(0, 3)) == MT_CLIENT_UPDATE)
				{
					// Get second username
					secondUserLen = stoi(msg.substr(3, 2));
					if (secondUserLen != 0) secName = msg.substr(5, secondUserLen);

					// Add content to msg
					currMsg = msg.substr(10 + secondUserLen, msg.length());
					if (currMsg != "")
					{
						currMsg = "&MAGSH_MESSAGE&&Author&" + username + "&DATA&" + currMsg;

						// Write to file
						std::string fileName = getFileName(username, secName);
						std::fstream outFile;
						outFile.open(fileName, std::fstream::app);

						if (outFile.fail()) throw std::exception("Failed Open the file");
						else
						{
							outFile << currMsg;
						}
					
					}


					msgContent += currMsg;


				}
				else if (getMessageTypeCode(msg.substr(0, 3)) == MT_CLIENT_EXIT)
				{
					if (username != "")
					{
						std::cout << username << " disconnected.\n";
						closesocket(clientSocket);
						this->_onlineUsers.erase(username);
					}
				}

				// Send update message
				hp.send_update_message_to_client(clientSocket, msgContent, secName, getAllUsers());

				// Lock
				std::unique_lock<std::mutex> locker(m1);
				// Pop message from queue
				this->_serverMessages.pop();
				// Unlock
				locker.unlock();
			}

		}
		catch (const std::exception& e)
		{
			closesocket(clientSocket);

			if (username != "") this->_onlineUsers.erase(username);	

			// Lock
			std::unique_lock<std::mutex> locker(m1);
			// Pop message from queue
			this->_serverMessages.pop();
			// Unlock
			locker.unlock();
		}
		
	}
}
/*
	std::unique_lock<std::mutex> locker(m1);
		cond.wait(locker);
*/

std::string Server::findUsernameBySocket(SOCKET socket)
{
	std::string username = "";

	for (auto& i : this->_onlineUsers) 
	{
		if (i.second == socket) 
		{
			username = i.first;
			break; // to stop searching
		}
	}

	return username;
}



boolean Server::fileExist(std::string name)
{
	std::ifstream infile(name);
	return infile.good();
}

std::string Server::getFileName(std::string firstName, std::string secName)
{
	std::string names[] = { firstName, secName };
	std::sort(names, names + (int)(sizeof(names) / sizeof(names[0])));

	std::string fileName = names[1] + "&" + names[0] + ".txt";

	return fileName;
}