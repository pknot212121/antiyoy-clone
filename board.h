#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include <random>

#include <unistd.h>

#include "sockets.h"

#define BIG_NUMBER 10000000

typedef int16_t coord;
typedef uint16_t ucoord;

// NIE ZMIENIAĆ KOLEJNOŚCI WARTOŚCI ANI NIE DODAWAĆ NOWYCH BEZ ZGODY
enum class Resident : uint8
{
    Water,
    Empty,

    Warrior1,
    Warrior2,
    Warrior3,
    Warrior4,

    Warrior1Moved,
    Warrior2Moved,
    Warrior3Moved,
    Warrior4Moved,

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
inline bool warrior(Resident resident) noexcept { return resident >= Resident::Warrior1 && resident <= Resident::Warrior4Moved; };
inline bool unmovedWarrior(Resident resident) noexcept { return resident >= Resident::Warrior1 && resident <= Resident::Warrior4; };
inline bool movedWarrior(Resident resident) noexcept { return resident >= Resident::Warrior1Moved && resident <= Resident::Warrior4Moved; };
inline bool building(Resident resident) noexcept { return resident >= Resident::Farm && resident <= Resident::StrongTower; };
inline bool farm(Resident resident) noexcept { return resident == Resident::Farm; };
inline bool castle(Resident resident) noexcept { return resident == Resident::Castle; };
inline bool tower(Resident resident) noexcept { return resident == Resident::Tower || resident == Resident::StrongTower; };
inline bool tree(Resident resident) noexcept { return resident == Resident::PalmTree || resident == Resident::PineTree; };
inline bool gravestone(Resident resident) noexcept { return resident == Resident::Gravestone; };

inline Resident move(Resident resident) noexcept { return (unmovedWarrior(resident)) ? (Resident)((int)resident + 4) : Resident::Empty; };
inline Resident unmove(Resident resident) noexcept { return (movedWarrior(resident)) ? (Resident)((int)resident - 4) : Resident::Empty; };


inline int8_t incomeBoard[] =
{
    0, //Water,
    0, //Empty, 0 bo 1 jest domyślnie liczony za każde pole

    -2, //Warrior1,
    -6, //Warrior2,
    -18, //Warrior3,
    -38, //Warrior4,

    -2, //Warrior1Moved,
    -6, //Warrior2Moved,
    -18, //Warrior3Moved,
    -36, //Warrior4Moved,

    4, //Farm,
    0, //Castle,
    -1, //Tower,
    -6, //StrongTower,

    -1, //PalmTree,
    -1, //PineTree,
    0, //Gravestone
};

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

int calculateIncome(std::vector<Hexagon*> hexagons);

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

    int price(Board* board, Resident resident);
    void rot(Board* board);
    int countFarms(Board* board);
    void setCastle(Board* board, int money);
    int removeCastle(Board* board);

    std::vector<Hexagon*> neighbours(Board* board, int recursion = 0, bool includeSelf = false, std::function<bool(Hexagon*)> filter = nullptr);
    std::vector<Hexagon*> doubleFilterNeighbours(Board* board, int recursion, bool includeSelf, std::function<bool(Hexagon*)> expansionFilter, std::function<bool(Hexagon*)> resultFilter);
    std::vector<Hexagon*> province(Board* board);
    std::vector<Hexagon*> calculateProvince(Board *board);
    int calculateProvinceIncome(Board* board);
    bool allows(Board* board, Resident resident, uint8 ownerId);
    std::vector<Hexagon*> possiblePlacements(Board* board, Resident resident);
    bool place(Board* board, Resident resident, Hexagon* placement);
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

    uint8 currentPlayerId = 1;

    std::vector<Country> countries;
    std::vector<uint8> leaderboard;

    Game* game;

public:
    // inicjalizatory
    Board(coord width, coord height, Game* game);
    void InitializeRandom(int min, int max);
    void InitializeNeighbour(int recursion, bool includeMiddle);
    void InitializeCountries(uint8 countriesCount, int minCountrySize, int maxCountrySize);
    //void InitializeFromFile();

    // gettery/settery
    inline coord getWidth() const noexcept { return width; }
    inline coord getHeight() const noexcept { return height; }
    inline Hexagon* getHexagon(coord x, coord y) { return (x < 0 || y < 0 || x >= width || y >= height) ? nullptr : &(board[y * width + x]); }
    inline Hexagon* getHexagon(int i) { return (i < 0 || i >= width * height) ? nullptr : &(board[i]); }
    std::unordered_set<Hexagon*> getHexesOfCountry(uint8 id); // z getterami do getterów bo wyrwę jaja i wygotuję w rosole

    inline Country* getCountry(uint8 id) noexcept { return (id == 0) ? nullptr : &countries[id-1]; }
    inline std::vector<Country>& getCountries() noexcept { return countries; }
    inline void leaderboardInsert(uint8 id) { leaderboard.insert(leaderboard.begin(), id); }

    inline const Game* getGame() const noexcept { return game; }
    inline int getCurrentPlayerId() const noexcept { return currentPlayerId; }

    void nextTurn(); // Definicja w game.cpp

    void sendBoard(int receivingSocket = -1);
    void sendGameOver(int receivingSocket = -1);
};

class Country
{
    Player* player;

    std::unordered_map<Hexagon*, int> castles; // zamki i pieniądze

public:
    Country(std::vector<Hexagon*> castles);

    inline std::unordered_map<Hexagon*, int>& getCastles() noexcept { return castles; }
    inline void setPlayer(Player* player) noexcept { this->player = player; }
};