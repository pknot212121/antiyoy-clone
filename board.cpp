#include "board.h"
#include <unordered_set>
#include <random>

Hexagon::Hexagon(coord x = 0, coord y = 0, uint8 ownerId = 0, Resident resident = Resident::Water) : x(x), y(y), ownerId(ownerId), resident(resident)
{

}

Board::Board(coord width, coord height) : width(width), height(height)
{
    board.reserve(width * height);
    for (coord x = 0; x < width; ++x)
        for (coord y = 0; y < height; ++y)
            board.emplace_back(x, y);
}

void Board::InitialiseRandomA(int seed, int min, int max)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> randN(min, max);
    int n = randN(gen);

    Hexagon* middle = getHexagon(getWidth() / 2, getHeight() / 2);
    if(!middle) return;

    std::unordered_set<Hexagon*> addableS = { middle };
    std::vector<Hexagon*> addableV = { middle };

    while(n > 0)
    {
        if(addableV.empty()) break;
        std::uniform_int_distribution<int> randomElement(0, addableV.size() - 1);
        int index = randomElement(gen);
        Hexagon* hex = addableV[index];
        addableS.erase(hex);
        addableV[index] = addableV.back();
        addableV.pop_back();
        hex->setResident(Resident::Empty);
        auto neighbours = hex->neighbours(this, 0, false, true);
        for(Hexagon* neighbour : neighbours)
        {
            if(neighbour->getResident() == Resident::Water && !addableS.count(neighbour))
            {
                addableS.insert(neighbour);
                addableV.push_back(neighbour);
            }
        }
        n--;
    }
}


std::vector<std::pair<coord, coord>> evenDirections =
{
    {-1, -1}, // lewy górny
    {-1,  0}, // lewy dolny
    { 0, -1}, // górny
    { 0,  1}, // dolny
    { 1, -1}, // prawy górny
    { 1,  0}  // prawy dolny
};

std::vector<std::pair<coord, coord>> oddDirections =
{
    {-1,  0}, // lewy górny
    {-1,  1}, // lewy dolny
    { 0, -1}, // górny
    { 0,  1}, // dolny
    { 1,  0}, // prawy górny
    { 1,  1}  // prawy dolny
};

void addNeighboursLayer(Board* board, std::unordered_set<Hexagon*>& visited, std::vector<Hexagon*>& hexagons, int recursion = 0, bool includeWater = false)
{
    std::vector<Hexagon*> newHexagons;
    newHexagons.reserve(hexagons.size() * 6);

    for(auto hexagon : hexagons)
    {
        coord x = hexagon->getX();
        coord y = hexagon->getY();
        auto& directions = (x % 2 == 0) ? evenDirections : oddDirections;

        for (auto [dx, dy] : directions)
        {
            Hexagon* hex = board->getHexagon(x + dx, y + dy); // getHexagon() robi sprawdzanie zakresów
            if(hex != nullptr && (includeWater || hex->getResident() != Resident::Water) && !visited.count(hex))
            {
                visited.insert(hex);
                newHexagons.push_back(hex);
            }
        }
    }
    if(recursion > 0) addNeighboursLayer(board, visited, newHexagons, recursion - 1, includeWater);
}

std::vector<Hexagon*> Hexagon::neighbours(Board* board, int recursion = 0, bool includeSelf = false, bool includeWater = false) // działa dla planszy takiej jak na obrazku board.jpg
{
    std::unordered_set<Hexagon*> visited = { this };
    std::vector<Hexagon*> newHexagons = { this };
    addNeighboursLayer(board, visited, newHexagons, recursion, includeWater);
    if(!includeSelf) visited.erase(this);
    std::vector<Hexagon*> res(visited.begin(), visited.end());
    return res;
}