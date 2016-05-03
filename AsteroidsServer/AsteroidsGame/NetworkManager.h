#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <iostream>
#include <winsock2.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <mutex>
//#include <sys/time.h>

#include "Thread.h"
#include "Game.h"

//give this to Egert when I get stuck
//This is the link to my thread manager example
//http://ashishchoure.blogspot.com/2010/05/simplest-threadpool-example-using-c.html

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define BUFLEN 512  //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data
#define MAX_CLIENTS 100 //max number of clients we can have attached

enum ObjType
{
	PLAYER_SHIP,
	PLAYER_LIGHT,
	ASTEROID_BIG,
	ASTEROID_MEDIUM,
	ASTEROID_SMALL,
	BULLET,
	LIGHT
};

enum Command
{
	UPDATE,
	PLAYER_DIED,
	BULLET_FIRED,
	PLAYER_DISCONNECT
};

struct header
{
	int id;
	Command cmd;
};

struct dataUpdate
{
	int numObj;
	struct
	{
		float xPos;
		float yPos;
		float rot;
		ObjType type;
	}* data;
};

struct dataFired
{
	float xPos;
	float yPos;
	float rot;
};

class NetworkManager
{
public:
	static void init();
	NetworkManager();
	~NetworkManager();
	int displayIP();
	int startServer();
	int sendData();
	int sendToAllClients();
	int receiveData();
	void updateData();
	void shutDownServer();

	//Thread Management
	void Initialize(int nThread);
	void AssignTask(void(*calback)());
	void ShutDownAllThreads();
	void ShutDownThread(DWORD dwThreadID);
	int GenerateID();
	int GetFreeThread();
	std::string GetTaskMessage();
	int GetThreadCount();
	HANDLE GetMutex();

	static Game* game;
	static NetworkManager* networkManager;

private:
	SOCKET s;
	struct sockaddr_in server, si_other;
	int slen, recv_len;
	char buf[BUFLEN];
	WSADATA wsa;
	bool runServer;
	int timeoutTime;
	std::vector<sockaddr_in> clients;
	std::mutex bufMutex;
	header* data;

	//Thread Management
	Thread* m_ptrThread[5];
	HANDLE m_hThreadPool[5];
	int m_nThreadCount;
	uint64_t ids;
};

