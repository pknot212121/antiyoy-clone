#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>

typedef short coord;
typedef unsigned char uint8;

enum class Resident : uint8
{
    Water, // woda ma mieć indeks 0
    Empty,

    Warrior1,
    Warrior2,
    Warrior3,
    Warrior4,

    Castle, // dziwne nazewnictwo z wiki
    Farm,
    Tower,
    StrongTower,

    PalmTree,
    PineTree,
    Gravestone
};

struct Point
{
    coord x;
    coord y;
};


class Hexagon; // deklaracje by nie było problemu z mieszaniem kolejności
class Board;
class Player;


class Hexagon
{
private:
    const coord x;
    const coord y;
    uint8 ownerId; // zakładamy że nie będzie więcej niż 255 graczy
    Resident resident; // enum o wymuszonym rozmiarze bajta
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
    std::vector<Hexagon*> neighbours(Board* board, int recursion = 0, bool includeSelf = false, std::function<bool(const Hexagon*)> filter = nullptr);
};

class Board
{
private:
    const coord width;
    const coord height;
    std::vector<Hexagon> board;

public:
    Board(coord width, coord height);
    void InitializeRandomA(int seed, int min, int max);
    void InitializeNeighbour(int recursion, bool includeMiddle);
    void InitializeCountriesA(int seed, uint8 countriesCount, int minCountrySize, int maxCountrySize);
    void InitializeFromFile();

    inline coord getWidth() const noexcept { return width; }
    inline coord getHeight() const noexcept { return height; }
    inline Hexagon* getHexagon(coord x, coord y) { if(x < 0 || y < 0 || x >= width || y >= height) return nullptr; return &(board[y * width + x]); }
    inline Hexagon* getHexagon(int i) { if(i < 0 || i >= width * height) return nullptr; return &(board[i]); }

};

// nie mam chwilowo pomysłu co dalej z nimi
class Player
{
    std::unordered_map<Hexagon*, int> castles; // zamki są znacznikami prowincji (każda prowincja ma dokładnie jeden zamek)
    virtual void move() = 0; // udawaj że to funkcja abstrakcyjna
};

class LocalPlayer : Player
{

};

class BotPlayer : Player
{

};

class NetworkPlayer : Player // Easter egg
{

};