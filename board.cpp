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
        auto neighbours = hex->neighbours(this, 0, false, [](Hexagon* h) { return h->getResident() == Resident::Water; });
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

std::vector<Hexagon*> Board::InitializeCountriesA(int seed, uint8 countriesCount, int minCountrySize, int maxCountrySize)
{
    std::vector<Hexagon*> origins;
    if(minCountrySize > maxCountrySize)
    {
        int t = maxCountrySize;
        maxCountrySize = minCountrySize;
        minCountrySize = t;
    }
    if(minCountrySize < 1) return origins;
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);

    int tries = 0;

restart:
    tries++;
    std::vector<Hexagon*> available;
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
        if (available.empty()) 
        {
            std::cout << "Not enough space to initialize countries" << std::endl;
            return origins;
        }

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
                    return origins;
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
            auto neighbours = hex->neighbours(this, 0, false, [](Hexagon* h) { return h->getResident() != Resident::Water && h->getOwnerId() == 0; });
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
    return origins;
}


void Hexagon::rot(Board* board)
{
    if(resident >= Resident::Warrior1 && resident <= Resident::Warrior4) setResident(Resident::Gravestone);
    else if((resident >= Resident::Castle && resident <= Resident::StrongTower) || resident == Resident::Gravestone)
    {
        if((neighbours(board, 0, false, [](Hexagon* h) { return h->resident == Resident::Water; })).size()) setResident(Resident::PalmTree);
        else setResident(Resident::PineTree);
    }
}


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

void addNeighboursLayer(Board* board, std::unordered_set<Hexagon*>& visited, std::vector<Hexagon*>& hexagons, int recursion, std::function<bool(Hexagon*)> filter)
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

std::vector<Hexagon*> Hexagon::neighbours(Board *board, int recursion, bool includeSelf, std::function<bool(Hexagon*)> filter)
{
    if (!filter) filter = [](Hexagon*) { return true; };
    std::unordered_set<Hexagon*> visited = { this };
    std::vector<Hexagon*> newHexagons = { this };
    addNeighboursLayer(board, visited, newHexagons, recursion, filter);
    if(!includeSelf) visited.erase(this);
    return std::vector<Hexagon*>(visited.begin(), visited.end());
}

// Droższa ale dokładniejsza od province(). Używać po zmianie terytorium by naprawić prowincje (dodać/odjąć zamki)
std::vector<Hexagon*> Hexagon::calculateProvince(Board* board)
{
    if(this->getOwnerId() == 0) return std::vector<Hexagon*>();
    std::vector<Hexagon*> province = this->neighbours(board, 10000000, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    int castlesNumber = 0;
    for(int i = 0; i < province.size(); i++)
    {
        if(province[i]->getResident() == Resident::Castle)
        {
            Hexagon* t = province[castlesNumber];
            province[castlesNumber] = province[i];
            province[i] = t;
            castlesNumber++;
        }
    }
    if(province.size() == 1)
    {
        if(castlesNumber == 0) province[0]->rot(board);
        return province;
    }
    if(castlesNumber > 1)
    {
        int mostMoney = 0;
        int bestI = 0;
        for(int i = 1; i < castlesNumber; i++)
        {
            // DODAĆ: znajdywanie zamku z największą ilością pieniędzy
        }
    }
    return province;
}

// Tańsza od calculateProvince() ale jedynie znajduje istniejącą prowincję. Używać do pozyskania prowincji która na pewno jest poprawna
std::vector<Hexagon*> Hexagon::province(Board* board)
{
    if(this->getOwnerId() == 0) return std::vector<Hexagon*>{ this };
    std::vector<Hexagon*> neighbours = this->neighbours(board, 10000000, true, [this](Hexagon* h) { return h->getOwnerId() == this->ownerId; });
    for(int i = 1; i < neighbours.size(); i++)
    {
        if(neighbours[i]->getResident() == Resident::Castle)
        {
            Hexagon* t = neighbours[0];
            neighbours[0] = neighbours[i];
            neighbours[i] = t;
            return neighbours;
        }
    }
    std::cout << "Castle not found" << std::endl;
    return neighbours;
}

// -1 - ląd, drzewa, groby
// 0 - farmy
// 1-4 - żołnierze i budowle
int power(Resident resident)
{
    if(resident == Resident::Farm) return 0;
    if(resident == Resident::Warrior1 || resident == Resident::Castle) return 1;
    if(resident == Resident::Warrior2 || resident == Resident::Tower) return 2;
    if(resident == Resident::Warrior3 || resident == Resident::StrongTower) return 3;
    if(resident == Resident::Warrior4) return 4;
    return -1;
}

// Sprawdza czy dany żołnierz może wejść na inne pole. Nie używać dla innych rezydentów
bool Hexagon::allows(Board* board, Resident warrior, uint8 ownerId)
{
    if(resident == Resident::Water) return false; // nie po wodzie
    if(this->ownerId == ownerId) return power(resident) < 0; // żołnierz może deptać po swoim lądzie, drzewach i grobach
    int attackerPower = power(warrior);
    if(attackerPower == 4) return true; // czwarty żołnierz rozjeżdża wszystko (włącznie z innymi czwartymi żołnierzami)
    std::vector<Hexagon*> neighbours = this->neighbours(board, 0, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    for(Hexagon* n : neighbours)
    {
        if(power(n->getResident()) >= attackerPower) return false; // jeśli ta płytka lub jej sąsiad ma kogoś o większej lub równej mocy to nie można wejść
    }
    return true;
}

// Używać dla żołnierzy, farm i wież
std::vector<Hexagon*> Hexagon::possiblePlacements(Board* board, Resident resident)
{
    if(ownerId == 0) return std::vector<Hexagon*>();
    if(resident >= Resident::Warrior1 && resident <= Resident::Warrior4)
    {
        return neighbours(board, 10000000, true, [this, board, resident](Hexagon* h)
        {
            if(h->ownerId == this->ownerId) return h->allows(board, resident, this->ownerId); // pola prowincji które pozwalają na tego żołnierza
            else return h->allows(board, resident, this->ownerId) && h->neighbours(board, 0, false, [this](Hexagon* h) { return h->ownerId == this->ownerId; }).size(); // obce pola wokół prowincji które pozwalają na tego żołnierza
        });
    }
    if(resident == Resident::Farm)
    {
        return neighbours(board, 10000000, true, [this, board](Hexagon* h) { return h->ownerId == this->ownerId && (h->resident == Resident::Empty || h->resident == Resident::Gravestone) && h->neighbours(board, 0, false, [this](Hexagon* h) { return h->ownerId == this->ownerId && (h->resident == Resident::Castle || h->resident == Resident::Farm); }).size(); }); // nasze pola które graniczą z innymi farmami lub zamkiem
    }
    if(resident == Resident::Tower)
    {
        return neighbours(board, 10000000, true, [this](Hexagon* h) { return h->ownerId == this->ownerId && (h->resident == Resident::Empty || h->resident == Resident::Gravestone); }); // nasze puste pola
    }
    if(resident == Resident::StrongTower)
    {
        return neighbours(board, 10000000, true, [this](Hexagon* h) { return h->ownerId == this->ownerId && (h->resident == Resident::Empty || h->resident == Resident::Gravestone || h->resident == Resident::Tower); }); // nasze puste pola lub zwykłe wieże
    }
    return std::vector<Hexagon*>();
}

bool Hexagon::move(Board* board, Hexagon* destination)
{
    if(resident >= Resident::Warrior1 && resident <= Resident::Warrior4)
    {
        if(destination->allows(board, resident, ownerId))
        {
            destination->setResident(resident);
            destination->setOwnerId(ownerId);
            setResident(Resident::Empty);
            return true;
        }
        return false;
    }
    return false;
}
