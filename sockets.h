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
    #include <unistd.h>
    #define INVALID_SOCKET -1
#endif

typedef unsigned char uint8;

#define MAGIC_SOCKET_TAG 0 // Magiczne numerki wysyłane na początku by mieć 100% pewności że jesteśmy poprawnie połączeni, wysyłane przez sendMagicNumbers()
#define CONFIGURATION_SOCKET_TAG 1 // Dane gry wysyłane przy rozpoczęciu nowej gry, wysyłane przez GameConfigData::sendGameConfigData()
#define BOARD_SOCKET_TAG 2 // Plansza (właściciele i rezydenci), wysyłana przez Board::sendBoard()
#define MOVE_SOCKET_TAG 3 // Lista ruchów gracza (DO ZROBIENIA)
#define CONFIRMATION_SOCKET_TAG 4 // Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu, wysyłane przez sendConfirmation()
#define TURN_CHANGE_SOCKET_TAG 5 // Numer gracza zaczynającego turę (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza), wysyłane przez sendTurnChange()
#define GAME_OVER_SOCKET_TAG 6 // Numery graczy w kolejności od wygranego do pierwszego który odpadł, wysyłane przez Board::sendGameOver()

#define SOCKET_MAGIC_NUMBERS { 'A', 'N', 'T', 'I', 'Y', 'O', 'Y' }

inline int sock = -1;
inline std::vector<int> clientSocks;

void initializeSocket(int port);
void acceptSocketClient(u_long mode = 0);
void searchForSocketClient(int discoveryPort);
void searchForServer(int discoveryPort, std::string* returnIp, int* returnPort);
inline bool invalidSocks() { return sock == -1 || clientSocks.empty(); }
void closeSockets();