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
    auto neighbours = middle->neighbours(this, recursion, includeMiddle);
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
        auto neighbours = hex->neighbours(this, 0, false, [](const Hexagon* h) { return h->getResident() == Resident::Water; });
        for(Hexagon* neighbour : neighbours)
        {
            if(!addableS.count(neighbour))
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
            auto neighbours = hex->neighbours(this, 0, false, [](const Hexagon* h) { return h->getResident() != Resident::Water && h->getOwnerId() == 0; });
            for(Hexagon* neighbour : neighbours)
            {
                if(!addableS.count(neighbour))
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


std::unordered_set<Hexagon*> Board::getHexesOfCountry(int countryID)
{
    std::unordered_set<Hexagon*> hexes;
    for (Hexagon &hex : board)
    {
        if (static_cast<int>(hex.getOwnerId())==countryID)
        {
            hexes.insert(&hex);
        }
    }
    return hexes;
}

void Board::addNeighboursLayer(Board* board, std::unordered_set<Hexagon*>& visited, std::vector<Hexagon*>& hexagons, int recursion, std::function<bool(const Hexagon*)> filter)
{
    if(hexagons.size() == 0) return;
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
            if(hex != nullptr && filter(hex) && !visited.count(hex))
            {
                visited.insert(hex);
                newHexagons.push_back(hex);
            }
        }
    }
    if(recursion > 0) addNeighboursLayer(board, visited, newHexagons, recursion - 1, filter);
}

void Board::addNeighboursLayer(std::unordered_set<Hexagon*>& visited, int recursion, std::function<bool(const Hexagon*)> filter)
{
    for(auto hexagon : visited)
    {
        coord x = hexagon->getX();
        coord y = hexagon->getY();
        auto& directions = (x % 2 == 0) ? evenDirections : oddDirections;

        for (auto [dx, dy] : directions)
        {
            Hexagon* hex = getHexagon(x + dx, y + dy); // getHexagon() robi sprawdzanie zakresów
            if(hex != nullptr && filter(hex) && !visited.count(hex))
            {
                visited.insert(hex);
            }
        }
    }
    if(recursion > 0) addNeighboursLayer(visited,recursion - 1, filter);
}


std::vector<Hexagon*> Hexagon::neighbours(Board* board, int recursion, bool includeSelf, std::function<bool(const Hexagon*)> filter)
{
    if (!filter) filter = [](const Hexagon*) { return true; };
    std::unordered_set<Hexagon*> visited = { this };
    std::vector<Hexagon*> newHexagons = { this };
    board->addNeighboursLayer(board, visited, newHexagons, recursion, filter);
    if(!includeSelf) visited.erase(this);
    return std::vector<Hexagon*>(visited.begin(), visited.end());
}
