#include "board.h"

Hexagon::Hexagon(coord x, coord y, uint8_t ownerId = 0, Resident resident = Resident::Empty) : x(x), y(y), ownerId(ownerId), resident(resident)
{

}

std::vector<Hexagon*> Hexagon::neighbours(Board* board) // działa dla planszy takiej jak na obrazku board.jpg
{
    std::vector<Hexagon*> res;
    coord maxX = board->getWidth() - 1;
    coord maxY = board->getHeight() - 1;
    if(x % 2 == 0) // parzysty x
    {
        if(x != 0 && y != 0) res.push_back(board->getHexagon(x - 1, y - 1)); // lewy górny
        if(x != 0) res.push_back(board->getHexagon(x - 1, y)); // lewy dolny
        if(y != 0) res.push_back(board->getHexagon(x, y - 1)); // górny
        if(y != maxY) res.push_back(board->getHexagon(x, y + 1)); // dolny
        if(x != maxX && y != 0) res.push_back(board->getHexagon(x + 1, y - 1)); // prawy górny
        if(x != maxX) res.push_back(board->getHexagon(x + 1, y)); // prawy dolny
    }
    else
    {
        if(x != 0) res.push_back(board->getHexagon(x - 1, y)); // lewy górny
        if(x != 0 && y != maxY) res.push_back(board->getHexagon(x - 1, y + 1)); // lewy dolny
        if(y != 0) res.push_back(board->getHexagon(x, y - 1)); // górny
        if(y != maxY) res.push_back(board->getHexagon(x, y + 1)); // dolny
        if(x != maxX) res.push_back(board->getHexagon(x + 1, y)); // prawy górny
        if(x != maxX && y != maxY) res.push_back(board->getHexagon(x + 1, y + 1)); // prawy dolny
    }
    return res;
}