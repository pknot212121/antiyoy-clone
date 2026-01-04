#include "sockets.h"
#include <sstream>
#include <cstring>

int getSocketError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif

}

void sleep(int time)
{
    #ifdef _WIN32
        Sleep(time);
    #else
        usleep(time*1000);
    #endif
}

void switchSocketMode(int sock, u_long mode)
{
#ifdef _WIN32
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (mode)
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    else
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

void sendData(char* data, int size, int receivingSocket, int exceptionSocket)
{
    if(invalidSocks()) return;
    if(clientSocks.size())
    {
        for(int s : clientSocks)
        {
            if((receivingSocket == -1 || s == receivingSocket) && s != -1 && s != exceptionSocket)
            {
                int sentBytes = 0;
                while (sentBytes < size)
                {
                    int r = send(s, data + sentBytes, size - sentBytes, 0);
                    if (r <= 0)
                    {
                        std::cout << "Failed to send data\n";
                        break;
                    }
                    sentBytes += r;
                }
            }
        }
    }
    else
    {
        if((receivingSocket == -1 || sock == receivingSocket) && sock != exceptionSocket)
        {
            int sentBytes = 0;
            while (sentBytes < size)
            {
                int r = send(sock, data + sentBytes, size - sentBytes, 0);
                if (r <= 0)
                {
                    std::cout << "Failed to send data\n";
                    break;
                }
                sentBytes += r;
            }
        }
    }
}

void initializeSocket(int port)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return;
    }
#endif

#ifdef _WIN32
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
#else
    sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cout << "Setsockopt failed\n";
    }
    if (sock < 0)
#endif
    {
        std::cout << "Failed to create socket\n";
        sock = -1;
        return;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&server, sizeof(server)) < 0)
    {
        std::cout << "Bind failed, error: " << getSocketError() << "\n";
        return;
    }

    if (listen(sock, 1) < 0)
    {
        std::cout << "Listen failed, error: " << getSocketError() << "\n";
        return;
    }

    #ifdef _WIN32
        u_long mode = 0;
        if (ioctlsocket(sock, FIONBIO, &mode) != 0)
        {
            std::cout << "Failed to set default socket mode! Error: " << getSocketError() << "\n";
        }
    #else
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            std::cout << "Failed to set default socket mode! Error: " << getSocketError() << "\n";
        }
    #endif
}

// 0 = blokujący (czeka na klienta), 1 = nieblokujący (jeśli nie ma klienta to kończy)
void acceptSocketClient(u_long mode)
{
    switchSocketMode(sock, mode);

    sockaddr_in client{};
#ifdef _WIN32
    int clientSize = sizeof(client);
#else
    unsigned int clientSize = sizeof(client);
#endif

    int clientSock = accept(sock, (sockaddr*)&client, &clientSize);

    switchSocketMode(sock, 0);
    if(clientSock == INVALID_SOCKET || clientSock < 0)
    {
        if (mode == 0) std::cout << "Accept failed, error: " << getSocketError() << "\n";
        return;
    }
    clientSocks.push_back(clientSock);
}

void searchForSocketClient(int discoveryPort, int tcpPort)
{
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);

    int broadcastEnable = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in bc{};
    bc.sin_family = AF_INET;
    bc.sin_port = htons(discoveryPort);
    bc.sin_addr.s_addr = INADDR_BROADCAST;

    std::string msg = "ANTIYOY " + std::to_string(tcpPort);

    int requiredClients = clientSocks.size() + 1;
    int warnCounter = 0;
    while (requiredClients > clientSocks.size()) // Jeśli liczba klientów się zmniejszy czekamy aż się nadrobi i wciąż na jednego więcej
    {
        if(sendto(udpSock, msg.c_str(), msg.length(), 0, (sockaddr*)&bc, sizeof(bc)) < 0)
        {
            if(++warnCounter % 10 == 0) std::cout << "Multiple sendto fails, will keep trying\n";
        }
        else warnCounter = 0;

        acceptSocketClient(1);

        sleep(300);
    }

    closeSocket(udpSock);
}

void searchForServer(int discoveryPort, std::string* returnIp, int* returnPort)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return;
    }
#endif
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(discoveryPort);
    local.sin_addr.s_addr = INADDR_ANY;

    if(bind(udpSock, (sockaddr*)&local, sizeof(local)) < 0)
    {
        std::cout << "bind error in searchForServer(), error: " << getSocketError() << '\n';
        closeSocket(udpSock);
        return;
    }

    char buffer[32];
    sockaddr_in sender;
    socklen_t senderLen = sizeof(sender);

    switchSocketMode(udpSock, 1);

    while(true)
    {
        int bytes = recvfrom(udpSock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sender, &senderLen);

        if (bytes <= 0)
        {
            sleep(200);
            continue;
        }

        buffer[bytes] = '\0';

        std::istringstream iss(buffer);
        std::string header;
        int port;

        if (iss >> header >> port)
        {
            if (header == "ANTIYOY")
            {
                *returnIp = inet_ntoa(sender.sin_addr);
                *returnPort = port;

                closeSocket(udpSock);
                return;
            }
        }
    }
}

bool connectToServer(std::string& ip, int port)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return false;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cout << "Failed to create TCP socket\n";
        sock = -1;
        return false;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

#ifdef _WIN32
    if (InetPtonA(AF_INET, ip.c_str(), &server.sin_addr) != 1)
    {
        std::cout << "Invalid IP address format\n";
        closeSocket(sock);
        sock = -1;
        return false;
    }
#else
    if (inet_pton(AF_INET, ip.c_str(), &server.sin_addr) != 1)
    {
        std::cout << "Invalid IP address format\n";
        closeSocket(sock);
        sock = -1;
        return false;
    }
#endif

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0)
    {
        std::cout << "Failed to connect to server! Error: " << getSocketError() << "\n";
        closeSocket(sock);
        sock = -1;
        return false;
    }

#ifdef _WIN32
    u_long mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == 1)
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    else
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

    return true;
}


void clearSocket(int sock)
{
    switchSocketMode(sock, 1);
    char buffer[512];
    while (recv(sock, buffer, sizeof(buffer), 0) > 0);
    switchSocketMode(sock, 0);
}

void closeSockets()
{
    for(int s : clientSocks)
    {
        closeSocket(s);
    }
    clientSocks.clear();
    closeSocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    sock = -1;
}



void sendMagicNumbers(int receivingSocket)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send magic numbers data\n";
        return;
    }
    char magicNumbers[] = SOCKET_MAGIC_NUMBERS;
    char content[1 + sizeof(magicNumbers)];
    content[0] = MAGIC_SOCKET_TAG;
    memcpy(content + 1, magicNumbers, sizeof(magicNumbers));
    sendData(content, sizeof(content), receivingSocket);
}

void sendConfirmation(bool approved, bool awaiting, int receivingSocket)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send confirmation data\n";
        return;
    }
    char content[3];
    content[0] = CONFIRMATION_SOCKET_TAG;
    content[1] = approved;
    content[2] = awaiting;
    sendData(content, sizeof(content), receivingSocket);
}

void sendTurnChange(uint8 player, int receivingSocket)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send turn change data\n";
        return;
    }
    char content[2];
    content[0] = TURN_CHANGE_SOCKET_TAG;
    content[1] = player;
    sendData(content, sizeof(content), receivingSocket);
}

void sendPlayerEliminated(uint8 player, int receivingSocket)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send turn change data\n";
        return;
    }
    char content[2];
    content[0] = PLAYER_ELIMINATED_SOCKET_TAG;
    content[1] = player;
    sendData(content, sizeof(content), receivingSocket);
}


bool receiveMagicNumbers(int deliveringSocket, bool tag)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot receive magic numbers data\n";
        return false;
    }
    if (deliveringSocket < 0) return false;

    if(tag)
    {
        char tag;
        if (recv(deliveringSocket, &tag, 1, 0) <= 0 || tag != MAGIC_SOCKET_TAG) return false;
    }

    char expectedMagic[] = SOCKET_MAGIC_NUMBERS;
    int expectedSize = sizeof(expectedMagic);
    char buffer[expectedSize];

    int total = 0;
    while (total < expectedSize)
    {
        int r = recv(deliveringSocket, buffer + total, expectedSize - total, 0);
        if (r <= 0) return false;
        total += r;
    }

    return memcmp(buffer, expectedMagic, expectedSize) == 0;
}


void runAi(std::string& pythonProgram, std::string& ipAddress, int port)
{
#ifdef _WIN32
    std::string cmd = "start python \"" + pythonProgram + ".py\" " + ipAddress + " " + std::to_string(port);
#elif __APPLE__
    std::string cmd = "osascript -e 'tell application \"Terminal\" to do script \"cd \\\"$(pwd)\\\" && python3 " + pythonProgram + ".py " + ipAddress + " " + std::to_string(port) + "\"'";
#else
    std::string cmd = "xterm -geometry 100x30 -e \"python3 " + pythonProgram + ".py " + ipAddress + " " + std::to_string(port) + "\" &";
#endif

    std::system(cmd.c_str());

    acceptSocketClient();
}