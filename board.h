#include <iostream>
#include <cstdint>
#include <vector>

typedef short coord;

enum class Resident : uint8_t
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


class Hexagon
{
private:
    const coord x;
    const coord y;
    uint8_t ownerId; // zakładamy że nie będzie więcej niż 255 graczy
    Resident resident; // enum o wymuszonym rozmiarze bajta
public:
    Hexagon(coord x, coord y, uint8_t ownerId, Resident resident);

    inline coord getX() const noexcept { return x; }
    inline coord getY() const noexcept { return y; }
    inline Resident getResident() const noexcept { return resident; }

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
    coord width;
    coord height;
    std::vector<Hexagon> board;

public:
    inline coord getWidth() const { return width; }
    inline coord getHeight() const { return height; }
    inline Hexagon* getHexagon(coord x, coord y) { if(x < 0 || y < 0 || x >= width || y >= height) return nullptr; return &(board[y * width + x]); }

};