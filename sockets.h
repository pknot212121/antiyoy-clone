#pragma once
#include <iostream>
#include <vector>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>

    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close

    inline void Sleep(unsigned int milliseconds) {
        usleep(milliseconds * 1000);
    }
#endif

typedef unsigned char uint8;

#define MAGIC_SOCKET_TAG 0 // Magiczne numerki wysyłane na początku by mieć 100% pewności że jesteśmy poprawnie połączeni, wysyłane przez sendMagicNumbers()
#define CONFIGURATION_SOCKET_TAG 1 // Dane gry wysyłane przy rozpoczęciu nowej gry, wysyłane przez GameConfigData::sendGameConfigData()
#define BOARD_SOCKET_TAG 2 // Plansza (właściciele i rezydenci), wysyłana przez Board::sendBoard()
#define ACTION_SOCKET_TAG 3 // Lista ruchów gracza (DO ZROBIENIA)
#define CONFIRMATION_SOCKET_TAG 4 // Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu, wysyłane przez sendConfirmation()
#define TURN_CHANGE_SOCKET_TAG 5 // Numer gracza zaczynającego turę (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza), wysyłane przez sendTurnChange()
#define GAME_OVER_SOCKET_TAG 6 // Numery graczy w kolejności od wygranego do pierwszego który odpadł, wysyłane przez Board::sendGameOver()

#define SOCKET_MAGIC_NUMBERS { 'A', 'N', 'T', 'I', 'Y', 'O', 'Y' }

inline int sock = -1;
inline std::vector<int> clientSocks;

inline u_long defaultSocketMode = 0;

int getSocketError();
void switchSocketMode(int sock, u_long mode);
void sendData(char* data, int size, int receivingSocket, int exceptionSocket = -1);
void initializeSocket(int port);
void acceptSocketClient(u_long mode = 0);
void searchForSocketClient(int discoveryPort, int tcpPort);
void searchForServer(int discoveryPort, std::string* returnIp, int* returnPort);
bool connectToServer(std::string& ip, int port);
void clearSocket(int sock);
inline bool invalidSocks() { return sock == -1; }
inline void closeSocket(int s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
void closeSockets();

void sendMagicNumbers(int receivingSocket = -1);
void sendConfirmation(bool approved, bool awaiting, int receivingSocket = -1);
void sendTurnChange(uint8 player, int receivingSocket = -1);

bool receiveMagicNumbers(int deliveringSocket, bool tag = false);

void runAi(std::string& pythonProgram, std::string& ipAddress, int port);