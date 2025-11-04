#include "board.h"
#include <unordered_set>
#include <random>

Hexagon::Hexagon() : x(0), y(0), ownerId(0), resident(Resident::Water){}

Hexagon::Hexagon(coord x, coord y) : x(x), y(y), ownerId(0), resident(Resident::Water){}

Hexagon::Hexagon(coord x, coord y, uint8 ownerId, Resident resident) : x(x), y(y), ownerId(ownerId), resident(resident)
{

}

Board::Board(coord width, coord height) : width(width), height(height)
{
    board.reserve(width * height);
    for (coord y = 0; y < height; y++)
    {
        for (coord x = 0; x < width; x++)
        {
            board.emplace_back(x, y);
        }
    }
}

void Board::InitializeNeighbour(int recursion, bool includeMiddle)
{
    Hexagon* middle = getHexagon(getWidth() / 2, getHeight() / 2);
    if(!middle) return;
    auto neighbours = middle->neighbours(this, recursion, includeMiddle, true);
    for(Hexagon* hex : neighbours)
    {
        hex->setResident(Resident::Empty);
    }
}

void Board::InitializeRandomA(int seed, int min, int max)
{
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);
    std::uniform_int_distribution<int> randN(min, max);
    int n = randN(gen);

    Hexagon* middle = getHexagon(getWidth() / 2, getHeight() / 2);
    if(!middle) return;

    std::unordered_set<Hexagon*> addableS = { middle };
    std::vector<Hexagon*> addableV = { middle };

    while(n > 0 && !addableV.empty())
    {
        std::uniform_int_distribution<int> randomAddable(0, addableV.size() - 1);
        int index = randomAddable(gen);
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

void Board::InitializeCountriesA(int seed, uint8 countriesCount, int minCountrySize, int maxCountrySize)
{
    if(minCountrySize > maxCountrySize)
    {
        int t = maxCountrySize;
        maxCountrySize = minCountrySize;
        minCountrySize = t;
    }
    if(minCountrySize < 1) return;
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);

    int tries = 0;

restart:
    tries++;
    std::vector<Hexagon*> available;
    std::vector<Hexagon*> origins;
    origins.clear();
    origins.reserve(countriesCount); // do dodania zamków na końcu (by nie musieć ich usuwać w przypadku restartu)
    
    for(uint8 i = 1; i <= countriesCount; i++)
    {
        available.clear();
        for(int j = 0; j < width * height; j++)
        {
            Hexagon* hex = &board[j];
            if(hex->getResident() != Resident::Water && hex->getOwnerId() == 0) available.push_back(hex);
        }
        if (available.empty()) return;

        std::uniform_int_distribution<int> randOrigin(0, available.size() - 1);
        Hexagon* origin = available[randOrigin(gen)];
        std::unordered_set<Hexagon*> addableS = { origin };
        std::vector<Hexagon*> addableV = { origin };
        origins.push_back(origin);

        std::uniform_int_distribution<int> randN(minCountrySize, maxCountrySize);
        int n = randN(gen);

        while(n > 0)
        {
            if(addableV.empty())
            {
                // To może się zdarzyć jeśli państwo zostanie zamknięte w ciasnym kącie przez inne państwa
                std::cout << "Couldnt initialize a country, restarting the process" << std::endl;
                for(int j = 0; j < width * height; j++)
                {
                    Hexagon* hex = &board[j];
                    if(hex->getResident() != Resident::Water && hex->getOwnerId() != 0) hex->setOwnerId(0);
                }
                if(tries > 100)
                {
                    std::cout << "Too many failed country initializations, aborted" << std::endl;
                    return;
                }
                else goto restart;
            }
            std::uniform_int_distribution<int> randomAddable(0, addableV.size() - 1);
            int index = randomAddable(gen);
            Hexagon* hex = addableV[index];
            addableS.erase(hex);
            addableV[index] = addableV.back();
            addableV.pop_back();
            hex->setOwnerId(i);
            auto neighbours = hex->neighbours(this);
            for(Hexagon* neighbour : neighbours)
            {
                if(neighbour->getOwnerId() == 0 && !addableS.count(neighbour))
                {
                    addableS.insert(neighbour);
                    addableV.push_back(neighbour);
                }
            }
            n--;
        }
    }

    for(Hexagon* o : origins)
    {
        o->setResident(Resident::Castle);
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

std::vector<Hexagon*> Hexagon::neighbours(Board* board, int recursion, bool includeSelf, bool includeWater)
{
    std::unordered_set<Hexagon*> visited = { this };
    std::vector<Hexagon*> newHexagons = { this };
    addNeighboursLayer(board, visited, newHexagons, recursion, includeWater);
    if(!includeSelf) visited.erase(this);
    std::vector<Hexagon*> res(visited.begin(), visited.end());
    return res;
}