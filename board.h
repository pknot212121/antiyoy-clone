#include <iostream>
#include <cstdint>
#include <vector>

typedef uint8_t coord;

enum class Resident : uint8_t
{
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
    coord x; // zakładamy że x ani y nie będą większe niż 256, można zmienić na short ale chyba nie jest to konieczne
    coord y;
};


class Hexagon
{
private:
    const coord x;
    const coord y;
    uint8_t ownerId; // zakładamy że nie będzie więcej niż 255 graczy
    Resident resident; // enum o wymuszonym rozmiarze bajta
public:
    Hexagon(coord x, coord y, uint8_t ownerId, Resident resident);
    std::vector<Hexagon*> neighbours(Board* board);
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
    inline Hexagon* getHexagon(coord x, coord y) { return &(board[y * width + x]); }

};