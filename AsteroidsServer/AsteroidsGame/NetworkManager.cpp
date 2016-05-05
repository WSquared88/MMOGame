#include "NetworkManager.h"

NetworkManager* NetworkManager::networkManager = nullptr;
Game* NetworkManager::game = nullptr;

void NetworkManager::Init()
{
	if (networkManager == nullptr)
	{
		networkManager = new NetworkManager();
	}
}

void NetworkManager::Initialize(int nThread)
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	if (nThread > sysinfo.dwNumberOfProcessors)
	{
		nThread = sysinfo.dwNumberOfProcessors;
		std::cout << "That is too many threads, the system can't handle it. Reducing to " << nThread << " threads instead." << std::endl;
	}

	m_nThreadCount = nThread;

	for (int i = 0; i < nThread; i++)
	{
		m_ptrThread[i] = new Thread();
		m_ptrThread[i]->CreateWorkerThread();
		m_hThreadPool[i] = m_ptrThread[i]->GetThreadHandle();
	}
}

NetworkManager::NetworkManager()
{
	slen = sizeof(si_other);
	head = (Header*)buf;

	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//Create a socket
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);
	timeoutTime = 5;
	runServer = true;
	//game = NULL;
}


NetworkManager::~NetworkManager()
{
	closesocket(s);
	WSACleanup();
}

int NetworkManager::DisplayIP()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return EXIT_FAILURE;
	}

	char ip[256];
	if (gethostname(ip, sizeof(ip)) == SOCKET_ERROR)
	{
		std::cout << "Unable to find the Host name of this computer" << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "The Host name of this computer is " << ip << std::endl;

	struct hostent* phe = gethostbyname(ip);

	if (!phe)
	{
		std::cout << "Unable to find the IP address of the computer" << std::endl;
		return EXIT_FAILURE;
	}

	for (int i = 0; phe->h_addr_list[i] != 0; ++i)
	{
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		std::cout << "Address " << i << ": " << inet_ntoa(addr) << std::endl;
	}

	return EXIT_SUCCESS;
}

int NetworkManager::StartServer()
{
	if (networkManager == nullptr)
	{
		return EXIT_FAILURE;
	}

	ids = 0;

	std::cout << "The thread started running successfully" << std::endl;
	//Bind
	if (bind(s, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	puts("Bind done");

	timeval tv;
	fd_set fds;

	//keep listening for data
	while (runServer)
	{
		printf("Waiting for data...\n");
		fflush(stdout);

		bufMutex.lock();
		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);
		bufMutex.unlock();

		std::cout << "attempting to receive data" << std::endl;

		tv.tv_sec = timeoutTime;
		tv.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET(s, &fds);

		/*if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		{
		perror("Error");
		}*/
		int timeoutError = select(s + 1, &fds, NULL, NULL, &tv);
		if (timeoutError == -1)
		{
			printf("Error trying to timeout\n");
		}
		else if (timeoutError == 0)
		{
			printf("Client timed out\n");
		}
		else
		{
			NetworkManager::networkManager->ReceiveData();
			NetworkManager::networkManager->UpdateData();
			NetworkManager::networkManager->SendData();
		}

		NetworkManager::networkManager->SendToAllClients();
	}

	delete networkManager;

	return 0;
}

int NetworkManager::SendData()
{
	bufMutex.lock();
	std::cout << "Sending ID " << head->id << std::endl;

	//now reply the client with the same data
	if (sendto(s, buf, BUFLEN, 0, (struct sockaddr*) &si_other, slen) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		return EXIT_FAILURE;
	}

	bufMutex.unlock();

	printf("Data sent to client \n");
	printf("Id sent to client: %d\n", ids);
	//std::cout << "si_other " << si_other << std::endl;
	return EXIT_SUCCESS;
}

int NetworkManager::SendToAllClients()
{
	bufMutex.lock();
	std::cout << "Pushing to all clients" << std::endl;
	head->cmd = UPDATE;
	DataUpdate* data = (DataUpdate*)(buf + sizeof(Header));
	//data->data = &ObjData();

	game->bufferMutex.lock();
	data->numObj = shipPos.size();// game->GetNumAliveShips();
	//data->data->pos = shipPos;// game->GetAliveShipPos();
	//data->data->rot = shipRot;// game->GetAliveShipRot();
	//data->data->type = PLAYER_SHIP;
	game->bufferMutex.unlock();
	//data->data = new ObjData[data->numObj];

	/*for (int i = 0; i < data->numObj; i++)
	{
		data->data[i].pos = tempPos[i];
		data->data[i].rot = tempRot[i];
		data->data[i].type = PLAYER_SHIP;
	}*/

	for (int i = 0; i < clients.size(); i++)
	{
		for (int j = 0; j < objs.size(); j++)
		{
			if (sendto(s, buf, sizeof(head) + sizeof(data), 0, (struct sockaddr*) &clients[i], slen) == SOCKET_ERROR)
			{
				printf("sendto() failed with error code : %d", WSAGetLastError());
				return EXIT_FAILURE;
			}
		}
	}

	std::cout << "Finished sending to all clients" << std::endl;
	bufMutex.unlock();

	return EXIT_SUCCESS;
}

int NetworkManager::ReceiveData()
{
	bufMutex.lock();
	//try to receive some data, this is a blocking call
	if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
	{
		printf("recvfrom() failed with error code : %d", WSAGetLastError());
		return EXIT_FAILURE;
	}

	//print details of the client/peer and the data received
	printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
	printf("Data: %s\n", buf);
	std::cout << "ID on receive: " << head->id << std::endl;

	if (head && head->cmd == PLAYER_DISCONNECT)
	{
		FreeID(head->id);
	}
	else if (head && head->id == 0)
	{
		head->id = GenerateID();
		std::cout << "Generated ID: " << (int)buf[0] << "\n" << std::endl;
	}
	else
	{
		std::cout << "There is an ID: " << head->id << "\n" << std::endl;
	}

	if (head->cmd == UPDATE)
	{
		DataUpdate* data = (DataUpdate*)(buf + sizeof(Header));
		int num = sizeof(Header);
		int num2 = sizeof(Header*);
		int num3 = sizeof(head);
		if (data)
		{
			bool foundObj = false;
			for (int i = 0; i < objs.size(); i++)
			{
				if (objs[i].id == data->data.id)
				{
					foundObj = true;
					objs[i] = data->data;
					break;
				}
			}

			if (!foundObj)
			{
				objs.push_back(data->data);
				// .push_back(data->data.pos);
				//shipRot.push_back(data->data.rot);
			}
		}
	}
	bufMutex.unlock();

	return EXIT_SUCCESS;
}

void NetworkManager::UpdateData()
{
	//change locations in buf once client is work on client is
	//finished and it is sending the proper data to server
	//game->shipAccelerationXBuffer[0] = (float)buf[];
	//game->shipAccelerationYBuffer[0] = (float)buf[];
	//game->lightAccelerationXBuffer[0] = (float)buf[];
	//game->lightAccelerationYBuffer[0] = (float)buf[];
	//game->asteroidVelocitiesXBuffer[0] = (float)buf[];
	//game->asteroidVelocitiesYBuffer[0] = (float)buf[];

}

void NetworkManager::ShutDownServer()
{
	runServer = false;
	ShutDownAllThreads();
}

void NetworkManager::ShutDownAllThreads()
{
	for (int i = 0; i < m_nThreadCount; i++)
	{
		m_ptrThread[i]->SignalShutDownEvent();
	}

	DWORD dwWaitResult = WaitForMultipleObjects(GetThreadCount(), m_hThreadPool, TRUE, INFINITE);

	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		for (int i = 0; i < m_nThreadCount; i++)
		{
			m_ptrThread[i]->ReleaseHandles();
			delete m_ptrThread[i];
		}

		break;
	default:
		std::cout << "Unable to close threads " << GetLastError() << std::endl;
	}
}

int NetworkManager::GenerateID()
{
	printf("ids %d", ids);
	clients.push_back(si_other);

	for (int i = 0; i < 64; i++)
	{

		if (!(ids & ((uint64_t)1 << i)))
		{
			ids |= (1 << i);
			return i+1;
		}
	}

	return -1;
}

void NetworkManager::FreeID(int id)
{
	ids &= ~(1 << (id-1));
	printf("ids after free %d", ids);
}

int NetworkManager::GetFreeThread()
{
	for (int i = 0; i < m_nThreadCount; i++)
	{
		if (m_ptrThread[i]->IsFree())
		{
			return i;
		}
		else
		{
			std::cout << "Thread " << i << ": " << m_ptrThread[i]->GetThreadID() << " is busy." << std::endl;
		}
	}

	std::cout << "All threads are busy. Try again later." << std::endl;
	return -1;
}

void NetworkManager::AssignTask(void(*callback)())
{
	int count = GetFreeThread();
	if (count != -1)
	{
		m_ptrThread[count]->SetThreadBusy();
		m_ptrThread[count]->SetFunction(callback);
		m_ptrThread[count]->SignalWorkEvent();
	}
}

int NetworkManager::GetThreadCount()
{
	return m_nThreadCount;
}