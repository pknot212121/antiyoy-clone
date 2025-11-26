#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <random>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif
#include <unistd.h>

#define BIG_NUMBER 10000000

typedef short coord;
typedef unsigned short ucoord;
typedef unsigned char uint8;

// NIE ZMIENIAĆ KOLEJNOŚCI WARTOŚCI ANI NIE DODAWAĆ NOWYCH BEZ ZGODY
enum class Resident : uint8
{
    Water,
    Empty,

    Warrior1,
    Warrior2,
    Warrior3,
    Warrior4,

    Farm, // dziwne nazewnictwo z wiki
    Castle,
    Tower,
    StrongTower,

    PalmTree,
    PineTree,
    Gravestone
};

inline bool water(Resident resident) noexcept { return resident == Resident::Water; };
inline bool empty(Resident resident) noexcept { return resident == Resident::Empty; };
inline bool warrior(Resident resident) noexcept { return resident >= Resident::Warrior1 && resident <= Resident::Warrior4; };
inline bool building(Resident resident) noexcept { return resident >= Resident::Farm && resident <= Resident::StrongTower; };
inline bool farm(Resident resident) noexcept { return resident == Resident::Farm; };
inline bool castle(Resident resident) noexcept { return resident == Resident::Castle; };
inline bool tower(Resident resident) noexcept { return resident == Resident::Tower || resident == Resident::StrongTower; };
inline bool tree(Resident resident) noexcept { return resident == Resident::PalmTree || resident == Resident::PineTree; };
inline bool gravestone(Resident resident) noexcept { return resident == Resident::Gravestone; };

/*struct Point
{
    coord x;
    coord y;
};*/

inline std::mt19937 gen; // generator liczb losowych

class Hexagon; // deklaracje by nie było problemu z mieszaniem kolejności
class Board;
class Player;
class Country;

class Game; // kosmita 👽👽👽

void markAll(std::vector<Hexagon*> hexagons);
void unmarkAll(std::vector<Hexagon*> hexagons);

#define MAGIC_SOCKET_TAG 0 // Magiczne numerki wysyłane na początku by mieć 100% pewności że jesteśmy poprawnie połączeni, wysyłane przez sendMagicNumbers()
#define CONFIGURATION_SOCKET_TAG 1 // Dane gry wysyłane przy rozpoczęciu nowej gry (DO ZROBIENIA)
#define BOARD_SOCKET_TAG 2 // Plansza (właściciele i rezydenci), wysyłana przez Board::sendBoard()
#define MOVE_SOCKET_TAG 3 // Lista ruchów gracza (DO ZROBIENIA)
#define CONFIRMATION_SOCKET_TAG 4 // Potwierdzenie wysyłane przez grę po otrzymaniu ruchu składające się z 2 booleanów: czy zatwierdzono ruch oraz czy nadal wyczekuje ruchu, wysyłane przez sendConfirmation()
#define TURN_CHANGE_SOCKET_TAG 5 // Numer gracza zaczynającego turę (zaczynając od 1, nie od 0 bo gra uznaje 0 za brak gracza), wysyłane przez sendTurnChange()
#define GAME_OVER_SOCKET_TAG 6 // Numery graczy w kolejności od wygranego do pierwszego który odpadł, wysyłane przez Board::sendGameOver()

#define SOCKET_MAGIC_NUMBERS { 'A', 'N', 'T', 'I', 'Y', 'O', 'Y' }

inline int sock = -1;
inline std::vector<int> clientSocks;

void initializeSocket(int port);
void awaitSocketClient();
inline bool invalidSocks() { return sock == -1 || clientSocks.empty(); }
void closeSocket();
void sendMagicNumbers(int receivingSocket = -1);
void sendConfirmation(bool approved, bool awaiting, int receivingSocket = -1);
void sendTurnChange(uint8 player, int receivingSocket = -1);


class Hexagon
{
private:
    const coord x;
    const coord y;
    uint8 ownerId; // zakładamy że nie będzie więcej niż 255 graczy
    Resident resident; // enum o wymuszonym rozmiarze bajta

    bool isMarked = false; // do renderowania, oznacza czy heks ma być zaznaczony czy nie
public:
    Hexagon();
    Hexagon(coord x, coord y);
    Hexagon(coord x, coord y, uint8 ownerId, Resident resident);

    inline coord getX() const noexcept { return x; }
    inline coord getY() const noexcept { return y; }
    inline uint8 getOwnerId() const noexcept { return ownerId; }
    inline void setOwnerId(uint8 ownerId) noexcept { this->ownerId = ownerId; }
    inline Resident getResident() const noexcept { return resident; }
    inline void setResident(Resident resident) noexcept { this->resident = resident; }

    void rot(Board* board);
    void setCastle(Board* board, int money);
    void setCastle(Board* board, int money, int farms);
    int removeCastle(Board* board);

    std::vector<Hexagon*> neighbours(Board* board, int recursion = 0, bool includeSelf = false, std::function<bool(Hexagon*)> filter = nullptr);
    std::vector<Hexagon*> doubleFilterNeighbours(Board* board, int recursion, bool includeSelf, std::function<bool(Hexagon*)> expansionFilter, std::function<bool(Hexagon*)> resultFilter);
    std::vector<Hexagon*> province(Board* board);
    std::vector<Hexagon*> calculateProvince(Board *board);
    bool allows(Board* board, Resident resident, uint8 ownerId);
    std::vector<Hexagon*> possiblePlacements(Board* board, Resident resident);
    std::vector<Hexagon *> possibleMovements(Board *board);
    bool move(Board *board, Hexagon *destination);

    inline void mark() noexcept { isMarked = true; }
    inline void unmark() noexcept { isMarked = false; }
    inline bool marked() const noexcept { return isMarked; }
};

class Board
{
private:
    const coord width;
    const coord height;
    std::vector<Hexagon> board;

    std::vector<Country> countries;
    std::vector<uint8> leaderboard;

    const Game* game;

public:
    // inicjalizatory
    Board(coord width, coord height, Game* game);
    void InitializeRandomA(int min, int max);
    void InitializeNeighbour(int recursion, bool includeMiddle);
    void InitializeCountriesA(uint8 countriesCount, int minCountrySize, int maxCountrySize);
    void InitializeFromFile();

    // gettery/settery
    inline coord getWidth() const noexcept { return width; }
    inline coord getHeight() const noexcept { return height; }
    inline Hexagon* getHexagon(coord x, coord y) { if(x < 0 || y < 0 || x >= width || y >= height) return nullptr; return &(board[y * width + x]); }
    inline Hexagon* getHexagon(int i) { if(i < 0 || i >= width * height) return nullptr; return &(board[i]); }
    std::unordered_set<Hexagon*> getHexesOfCountry(int countryID); // z getterami do getterów bo wyrwę jaja i wygotuję w rosole

    inline Country* getCountry(uint8 id) noexcept { if(id == 0) return nullptr; return &countries[id-1]; }
    inline std::vector<Country>& getCountries() noexcept { return countries; }

    inline const Game* getGame() const noexcept { return game; }

    void sendBoard(int receivingSocket = -1);
    void sendGameOver(int receivingSocket = -1);
};

struct MoneyAndFarms
{
public:
    int money;
    int farms;
};

class Country
{
    Player* player;

    std::unordered_map<Hexagon*, MoneyAndFarms> castles; // zamki, pieniądze i liczba farm

public:
    Country(std::vector<Hexagon*> castles);

    inline std::unordered_map<Hexagon*, MoneyAndFarms>& getCastles() noexcept { return castles; }
    inline void setPlayer(Player* player) noexcept { this->player = player; }
};

class Player
{
    Country* country;
    unsigned int maxMoveTime;

    //virtual void move() = 0; // udawaj że to funkcja abstrakcyjna

public:
    Player(Country* country, unsigned int maxMoveTime = 60);
};

class LocalPlayer : public Player
{
public:
    LocalPlayer(Country* country, unsigned int maxMoveTime = 60);
};

class BotPlayer : public Player
{
public:
    BotPlayer(Country* country, unsigned int maxMoveTime = 10);
};

class NetworkPlayer : Player // Easter egg
{

};