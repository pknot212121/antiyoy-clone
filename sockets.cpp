#include "sockets.h"
#include <sstream>

int getSocketError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif

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

    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0)
    {
        std::cout << "Failed to set non-blocking mode! Error: " << getSocketError() << "\n";
    }
}

// 0 = blokujący (czeka na klienta), 1 = nieblokujący (jeśli nie ma klienta to kończy)
void acceptSocketClient(u_long mode)
{
    ioctlsocket(sock, FIONBIO, &mode);

    sockaddr_in client{};
#ifdef _WIN32
    int clientSize = sizeof(client);
#else
    unsigned int clientSize = sizeof(client);
#endif

    int clientSock = accept(sock, (sockaddr*)&client, &clientSize);

    if (mode == 0 && (clientSock == INVALID_SOCKET || clientSock < 0))
    {
        std::cout << "Accept failed, error: " << getSocketError() << "\n";
        return;
    }
    clientSocks.push_back(clientSock);
}

void searchForSocketClient(int discoveryPort)
{
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);

    int broadcastEnable = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in bc{};
    bc.sin_family = AF_INET;
    bc.sin_port = htons(discoveryPort);
    bc.sin_addr.s_addr = INADDR_BROADCAST;

    std::string msg = "ANTIYOY " + std::to_string(discoveryPort);

    int requiredClients = clientSocks.size() + 1;
    while (requiredClients > clientSocks.size()) // Jeśli liczba klientów się zmniejszy czekamy aż się nadrobi i wciąż na jednego więcej
    {
        sendto(udpSock, msg.c_str(), msg.length(), 0, (sockaddr*)&bc, sizeof(bc));

        acceptSocketClient(1);

        Sleep(300);
    }

    closesocket(udpSock);
}

void searchForServer(int discoveryPort, std::string* returnIp, int* returnPort)
{
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(discoveryPort);
    local.sin_addr.s_addr = INADDR_ANY;

    bind(udpSock, (sockaddr*)&local, sizeof(local));

    char buffer[32];
    sockaddr_in sender;
    socklen_t senderLen = sizeof(sender);

    while(true)
    {
        int bytes = recvfrom(udpSock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sender, &senderLen);

        if (bytes <= 0) continue;

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

                closesocket(udpSock);
                return;
            }
        }
    }
}

void closeSockets()
{
#ifdef _WIN32
    for(int s : clientSocks)
    {
        closesocket(s);
    }
    clientSocks.clear();
    closesocket(sock);
    WSACleanup();
#else
    for(int s : clientSocks)
    {
        close(s);
    }
    clientSocks.clear();
    close(sock);
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
    for(int s : clientSocks)
    {
        if(receivingSocket == -1 || s == receivingSocket)
        {
            int sentBytes = 0;
            while (sentBytes < sizeof(content))
            {
                int r = send(s, content + sentBytes, sizeof(content) - sentBytes, 0);
                if (r <= 0)
                {
                    std::cout << "Failed to send magic numbers data\n";
                    break;
                }
                sentBytes += r;
            }
        }
    }
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
    for(int s : clientSocks)
    {
        if(receivingSocket == -1 || s == receivingSocket)
        {
            int sentBytes = 0;
            while (sentBytes < sizeof(content))
            {
                int r = send(s, content + sentBytes, sizeof(content) - sentBytes, 0);
                if (r <= 0)
                {
                    std::cout << "Failed to send confirmation data\n";
                    break;
                }
                sentBytes += r;
            }
        }
    }
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
    for(int s : clientSocks)
    {
        if(receivingSocket == -1 || s == receivingSocket)
        {
            int sentBytes = 0;
            while (sentBytes < sizeof(content))
            {
                int r = send(s, content + sentBytes, sizeof(content) - sentBytes, 0);
                if (r <= 0)
                {
                    std::cout << "Failed to send turn change data\n";
                    break;
                }
                sentBytes += r;
            }
        }
    }
}