#pragma once
#include <iostream>
#include <vector>

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
class Country;
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
    inline Resident getResident() const noexcept { return resident; }
    inline void setResident(Resident resident) noexcept { this->resident = resident; }

    std::vector<Hexagon*> neighbours(Board* board, int recursion, bool includeSelf, bool includeWater);
};

class Country
{
private:
    std::vector<Point> castlePositions; // zamki są znacznikami prowincji (każda prowincja ma dokładnie jeden zamek)
public:

};

class Board
{
private:
    const coord width;
    const coord height;
    std::vector<Hexagon> board;

public:
    Board(coord width, coord height);
    void InitialiseRandomA(int seed, int min, int max);
    void InitialiseRandomB(int seed, int min, int max);
    void InitialiseFromFile();

    inline coord getWidth() const { return width; }
    inline coord getHeight() const { return height; }
    inline Hexagon* getHexagon(coord x, coord y) { if(x < 0 || y < 0 || x >= width || y >= height) return nullptr; return &(board[y * width + x]); }

};

// nie mam chwilowo pomysłu co dalej z nimi
/*class Player
{
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

};*/