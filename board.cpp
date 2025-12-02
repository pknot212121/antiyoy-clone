#include "board.h"
#include <unordered_set>
#include <cstring>
#include <set>
#include <algorithm>


void markAll(std::vector<Hexagon *> hexagons)
{
    for(Hexagon* h : hexagons) h->mark();
}

void unmarkAll(std::vector<Hexagon*> hexagons)
{
    for(Hexagon* h : hexagons) h->unmark();
}


// -1 - ląd, drzewa, groby
// 0 - farmy
// 1-4 - żołnierze i budowle
int power(Resident resident)
{
    if(resident == Resident::Farm) return 0;
    if(resident == Resident::Warrior1 || resident == Resident::Warrior1Moved || resident == Resident::Castle) return 1;
    if(resident == Resident::Warrior2 || resident == Resident::Warrior2Moved || resident == Resident::Tower) return 2;
    if(resident == Resident::Warrior3 || resident == Resident::Warrior3Moved || resident == Resident::StrongTower) return 3;
    if(resident == Resident::Warrior4 || resident == Resident::Warrior4Moved) return 4;
    return -1;
}

int Hexagon::price(Board* board, Resident resident)
{
    Hexagon* castleHex;
    if(castle(getResident())) castleHex = this;
    else castleHex = this->province(board)[0];
    if(!castle(castleHex->resident)) return 0;
    if(unmovedWarrior(resident)) return power(resident) * 10;
    if(farm(resident)) return 12 + 2 * countFarms(board); // + board->getCountry(ownerId)->getCastles()[castleHex].farms * 2
    if(resident == Resident::Tower) return 15;
    if(resident == Resident::StrongTower) return 35;
    return 0;
}

// Zwraca połączenie żołnierzy. Jeśli nie mogą być połączeni zwraca Resident::Empty
Resident mergeWarriors(Resident warrior1, Resident warrior2)
{
    if(!(warrior(warrior1) && warrior(warrior2))) return Resident::Empty;
    int sum = power(warrior1) + power(warrior2);
    if(sum > 4) return Resident::Empty;
    return (Resident)((int)Resident::Warrior1 - 1 + sum + 4 * (movedWarrior(warrior1) || movedWarrior(warrior2)));
}

int calculateIncome(std::vector<Hexagon*> hexagons)
{
    int total = 0;
    for(Hexagon* h : hexagons)
    {
        total += incomeBoard[(int)h->getResident()] + 1;
    }

    return total;
}

Hexagon::Hexagon() : x(0), y(0), ownerId(0), resident(Resident::Water){}

Hexagon::Hexagon(coord x, coord y) : x(x), y(y), ownerId(0), resident(Resident::Water){}

Hexagon::Hexagon(coord x, coord y, uint8 ownerId, Resident resident) : x(x), y(y), ownerId(ownerId), resident(resident)
{

}

Board::Board(coord width, coord height, Game* game) : width(width), height(height), game(game)
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


void Board::InitializeRandom(int min, int max)
{
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
        auto neighbours = hex->neighbours(this, 0, false, [](Hexagon* h) { return water(h->getResident()); });
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

void Board::InitializeCountries(uint8 countriesCount, int minCountrySize, int maxCountrySize)
{
    if(minCountrySize > maxCountrySize)
    {
        int t = maxCountrySize;
        maxCountrySize = minCountrySize;
        minCountrySize = t;
    }
    if(minCountrySize < 1) return;

    int tries = 0;

restart:
    tries++;
    std::vector<Hexagon*> available;
    std::vector<Hexagon*> origins;
    origins.clear();
    origins.reserve(countriesCount);
    
    for(uint8 i = 1; i <= countriesCount; i++)
    {
        available.clear();
        for(int j = 0; j < width * height; j++)
        {
            Hexagon* hex = &board[j];
            if(!water(hex->getResident()) && hex->getOwnerId() == 0) available.push_back(hex);
        }
        if (available.empty()) 
        {
            std::cout << "Not enough space to initialize countries" << std::endl;
            return;
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
                    if(!water(hex->getResident()) && hex->getOwnerId() != 0) hex->setOwnerId(0);
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
            auto neighbours = hex->neighbours(this, 0, false, [](Hexagon* h) { return !water(h->getResident()) && h->getOwnerId() == 0; });
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

    countries.reserve(countriesCount);

    for(Hexagon* o : origins)
    {
        countries.emplace_back(std::vector{ o });
    }
    lastPlayerId = countriesCount;
}

bool Hexagon::isNearWater(Board *board)
{
    return (neighbours(board, 0, false, [](Hexagon* h) { return water(h->resident); })).size() > 0;
}

bool Hexagon::bordersPineAndOtherTree(Board *board)
{
    bool bordersPine = (neighbours(board, 0, false, [](Hexagon* h) { return h->getResident()==Resident::PineTree; })).size()>=1;
    bool bodrdersTwoTrees = (neighbours(board, 0, false, [](Hexagon* h) { return tree(h->getResident()); })).size()>=2;
    return bordersPine && bodrdersTwoTrees;
}

void Board::spawnTrees(double treeRatio)
{
    int count = board.size();
    std::vector<int> range(count);
    std::iota(range.begin(), range.end(), 0);
    std::erase_if(range,[this](int i){return this->board[i].getResident()!=Resident::Empty;});

    std::shuffle(range.begin(),range.end(),gen);
    for (int i=0;i<range.size()*treeRatio;i++)
    {
        if (board[range[i]].isNearWater(this)) board[range[i]].setResident(Resident::PalmTree);
        else board[range[i]].setResident(Resident::PineTree);

    }
}


void Board::nextTurn(bool send) // Definicja przeniesiona tutaj ze względu na game->getPlayer()
{
    if (getCountry(lastPlayerId)->getCastles().size() <= 0)
    {
        for (int i = lastPlayerId - 1; i >= 1; i--)
        {
            if (getCountry(i)->getCastles().size() > 0)
            {
                lastPlayerId = i;
                break;
            }
        }
    }
    //std::erase_if(leaderboard,[this](uint8 index){return getCountry(index)->getCastles().size() >0;});

    std::unordered_map<Hexagon*, int>& oldCastles = getCountry(currentPlayerId)->getCastles();
    for (auto& [caslteHex, money] : oldCastles)
    {
        std::vector<Hexagon*> province = caslteHex->neighbours(this, BIG_NUMBER, true, [caslteHex](Hexagon* h) { return h->getOwnerId() == caslteHex->getOwnerId(); });
        for(Hexagon* h : province)
        {
            if(unmovedWarrior(h->getResident())) h->setResident(move(h->getResident()));
        }
    }
    if (currentPlayerId == lastPlayerId) propagateTrees();

    uint8 oldId = currentPlayerId;

    bool retry = true;
    while(retry) // Szukamy gracza który jeszcze żyje
    {
        currentPlayerId = currentPlayerId % countries.size() + 1;
        retry = getCountries()[currentPlayerId].getCastles().size() == 0; // Jeśli nie ma zamków to powtarzamy
    }

    std::unordered_map<Hexagon*, int>& castles = getCountry(currentPlayerId)->getCastles();
    for (auto& [caslteHex, money] : castles)
    {
        std::vector<Hexagon*> province = caslteHex->neighbours(this, BIG_NUMBER, true, [caslteHex](Hexagon* h) { return h->getOwnerId() == caslteHex->getOwnerId(); });
        for(Hexagon* h : province)
        {
            if(movedWarrior(h->getResident())) h->setResident(unmove(h->getResident()));
            if (gravestone(h->getResident()))
            {
                if (h->isNearWater(this)) h->setResident(Resident::PalmTree);
                else h->setResident(Resident::PineTree);
            }
        }
        money += calculateIncome(province);

        if (money<0)
        {
            for (Hexagon *h : province)
            {
                if (unmovedWarrior(h->getResident())) h->setResident(Resident::Gravestone);
            }
        }

    }

    if(send)
    {
        char content[3]; // tag (1) + liczba akcji (1) + tag akcji (1)
        char* position = content;

        *position++ = ACTION_SOCKET_TAG; // tag
        *position++ = 1; // liczba akcji (jedna)
        *position++ = 0; // tag akcji (0 - koniec tury)
        sendData(content, sizeof(content), -1);
        //std::cout << "Sent next turn\n";
    }

    //game->getPlayer(currentPlayerId)->actStart();
}


void Board::propagateTrees()
{
    std::uniform_real_distribution<double> chanceDist(0.0,1.0);
    std::set<Hexagon*> palms;
    std::set<Hexagon*> pines;
    for (Hexagon& h : board)
    {
        if (tree(h.getResident()))
        {
            std::vector<Hexagon*> neigh = h.neighbours(this,0,false);
            std::erase_if(neigh, [](Hexagon* hex){return hex->getResident()!=Resident::Empty;});

            if (neigh.size()>0)
            {
                double chance = chanceDist(gen);

                if (h.getResident()==Resident::PalmTree && chance<=0.3)
                {
                    std::erase_if(neigh, [this](Hexagon* hex){return !hex->isNearWater(this);});
                    if (neigh.size()>0)
                    {
                        std::uniform_int_distribution<size_t> neighborDist(0, neigh.size() - 1);
                        int choice = round(neighborDist(gen));
                        palms.insert(neigh[choice]);
                    }
                }
                else if (h.getResident()==Resident::PineTree && chance<=0.2)
                {
                    std::erase_if(neigh, [this](Hexagon* hex){return !hex->bordersPineAndOtherTree(this);});
                    if (neigh.size()>0)
                    {
                        std::uniform_int_distribution<size_t> neighborDist(0, neigh.size() - 1);
                        int choice = round(neighborDist(gen));
                        pines.insert(neigh[choice]);
                    }
                }
            }

        }
    }
    for (Hexagon* h : pines) h->setResident(Resident::PineTree);
    for (Hexagon* h : palms) h->setResident(Resident::PalmTree);
}

void Board::sendBoard(int receivingSocket)
{
    if(invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send board data\n";
        return;
    }
    int hexesNumber = width * height;
    ucoord wh = htons(width);
    ucoord hh = htons(height);
    int total = 1 + sizeof(wh) + sizeof(hh) + hexesNumber * 2;
    uint8* result = new uint8[total];
    result[0] = BOARD_SOCKET_TAG;
    uint8* position = result + 1;
    memcpy(position, &wh, sizeof(wh));
    position += sizeof(wh);
    memcpy(position, &hh, sizeof(hh));
    position += sizeof(hh);
    for(int i = 0; i < hexesNumber; i++)
    {
        position[i * 2] = board[i].getOwnerId();
        position[i * 2 + 1] = static_cast<uint8>(board[i].getResident());
    }

    sendData(reinterpret_cast<char*>(result), total, receivingSocket);
    
    delete[] result;
}

void Board::sendGameOver(int receivingSocket)
{
    if(invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send game over data\n";
        return;
    }
    int lbSize = leaderboard.size();
    int total = 2 + lbSize;
    uint8* lb = new uint8[total];
    lb[0] = GAME_OVER_SOCKET_TAG;
    lb[1] = lbSize;
    memcpy(lb + 2, leaderboard.data(), lbSize);

    sendData(reinterpret_cast<char*>(lb), total, receivingSocket);
    
    delete[] lb;
}



void Hexagon::rot(Board* board)
{
    if(warrior(resident)) setResident(Resident::Gravestone);
    else if((resident >= Resident::Castle && resident <= Resident::StrongTower) || gravestone(resident))
    {
        if((neighbours(board, 0, false, [](Hexagon* h) { return water(h->resident); })).size()) setResident(Resident::PalmTree);
        else setResident(Resident::PineTree);
    }
}

void Hexagon::rotOnlyTrees(Board* board)
{
    if(gravestone(resident))
    {
        if((neighbours(board, 0, false, [](Hexagon* h) { return water(h->resident); })).size()) setResident(Resident::PalmTree);
        else setResident(Resident::PineTree);
    }
}

int Hexagon::countFarms(Board* board)
{
    auto province = neighbours(board, BIG_NUMBER, false, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    int farmsNumber = 0;
    for(Hexagon* h : province)
    {
        if(farm(h->getResident())) farmsNumber++;
    }
    return farmsNumber;
}

void Hexagon::setCastle(Board* board, int money)
{
    auto& castlesMap = board->getCountry(ownerId)->getCastles();
    resident = Resident::Castle;
    castlesMap[this] = money;
}



std::unordered_set<Hexagon*> Board::getHexesOfCountry(uint8 id)
{
    std::unordered_set<Hexagon*> hexes;
    for (Hexagon &hex : board)
    {
        if (hex.getOwnerId() == id)
        {
            hexes.insert(&hex);
        }
    }
    return hexes;
}

std::vector<std::pair<coord, coord>> evenDirections =
{
    { 0, -1}, // górny
    {-1, -1}, // lewy górny
    {-1,  0}, // lewy dolny
    { 0,  1}, // dolny
    { 1,  0}, // prawy dolny
    { 1, -1}  // prawy górny
};

std::vector<std::pair<coord, coord>> oddDirections =
{
    { 0, -1}, // górny
    {-1,  0}, // lewy górny
    {-1,  1}, // lewy dolny
    { 0,  1}, // dolny
    { 1,  1}, // prawy dolny
    { 1,  0}  // prawy górny
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

void doubleFilterAddNeighboursLayer(Board* board, std::unordered_set<Hexagon*>& visited, std::vector<Hexagon*>& hexagons, int recursion, std::function<bool(Hexagon*)> expansionFilter, std::function<bool(Hexagon*)> resultFilter)
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
            if(hex != nullptr && !visited.count(hex) && expansionFilter(hex))
            {
                newHexagons.push_back(hex);
                if(resultFilter(hex)) visited.insert(hex);
            }
        }
    }
    if(recursion > 0) doubleFilterAddNeighboursLayer(board, visited, newHexagons, recursion - 1, expansionFilter, resultFilter);
}

// Działa podobnie do neighbours() ale ma dwa filtry: rozrostu (które heksy analizujemy przy następnej iteracji) i wyniku (co zostanie zwrócone)
std::vector<Hexagon*> Hexagon::doubleFilterNeighbours(Board *board, int recursion, bool includeSelf, std::function<bool(Hexagon*)> expansionFilter, std::function<bool(Hexagon*)> resultFilter)
{
    if (!expansionFilter) expansionFilter = [](Hexagon*) { return true; };
    if (!resultFilter) resultFilter = [](Hexagon*) { return true; };
    std::unordered_set<Hexagon*> visited = { this };
    std::vector<Hexagon*> newHexagons = { this };
    doubleFilterAddNeighboursLayer(board, visited, newHexagons, recursion, expansionFilter, resultFilter);
    if(!includeSelf) visited.erase(this);
    return std::vector<Hexagon*>(visited.begin(), visited.end());
}

// Droższa ale dokładniejsza od province(). Używać po zmianie terytorium by naprawić prowincje (dodać/odjąć zamki)
std::vector<Hexagon*> Hexagon::calculateProvince(Board* board)
{
    std::cout << "Calculate province for " << (int)ownerId << "\n";
    if(this->getOwnerId() == 0) return std::vector<Hexagon*>();
    std::vector<Hexagon*> province = this->neighbours(board, BIG_NUMBER, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    int castlesNumber = 0;
    for(int i = 0; i < province.size(); i++)
    {
        if(castle(province[i]->getResident()))
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
        std::unordered_map<Hexagon*, int> castlesMap = board->getCountry(ownerId)->getCastles();
        /*int mostFarms = 0;
        int mostMoney = 0;
        int bestI = 0;
        for(int i = 0; i < castlesNumber; i++)
        {
            MoneyAndFarms moneyAndFarms = castlesMap[province[i]];
            if(moneyAndFarms.farms > mostFarms)
            {
                bestI = i;
                mostMoney = moneyAndFarms.money;
                mostFarms = moneyAndFarms.farms;
            }
            else if(moneyAndFarms.farms == mostFarms)
            {
                if(moneyAndFarms.money > mostMoney)
                {
                    bestI = i;
                    mostMoney = moneyAndFarms.money;
                    mostFarms = moneyAndFarms.farms;
                }
            }
        }
        if(bestI != 0)
        {
            Hexagon* t = province[bestI];
            province[bestI] = province[0];
            province[0] = t;
        }*/
        for(int i = 1; i < castlesNumber; i++)
        {
            castlesMap[province[0]] += province[i]->removeCastle(board, false);
        }
        return province;
    }
    if(castlesNumber == 0)
    {
        std::vector<Hexagon*> firstLine;
        firstLine.reserve(province.size());
        std::vector<Hexagon*> secondLine;
        secondLine.reserve(province.size());
        for(Hexagon* h : province)
        {
            if(empty(h->getResident())) firstLine.push_back(h);
            else secondLine.push_back(h);
        }
        
        Hexagon* newCastle;
        if(firstLine.size()) // w pierwszej kolejności próbuje położyć zamek na pustym polu
        {
            std::uniform_int_distribution<int> rand(0, firstLine.size() - 1);
            newCastle = firstLine[rand(gen)];
        }
        else // potem na niepustym
        {
            std::uniform_int_distribution<int> rand(0, secondLine.size() - 1);
            newCastle = secondLine[rand(gen)];
        }
        if (board->getCountry(ownerId)->tempMoneyStorage==0) newCastle->setCastle(board, 0);
        else
        {
            newCastle->setCastle(board,board->getCountry(ownerId)->tempMoneyStorage);
            board->getCountry(ownerId)->tempMoneyStorage=0;
        }

    }
    return province;
}

bool Hexagon::isNextToTowerOrCastle(Board *board,uint8 id)
{
    return (neighbours(board, 0, false, [id](Hexagon* h) { return (tower(h->resident) || castle(h->resident)) && h->getOwnerId()==id; })).size() > 0 && !tower(resident) && !castle(resident);
}

std::unordered_set<Hexagon*> Hexagon::getAllProtectedAreas(Board* board)
{
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(getOwnerId());
    std::erase_if(hexes,[board,this](Hexagon *h){return !h->isNextToTowerOrCastle(board,getOwnerId());});
    return hexes;
}

int Hexagon::calculateProvinceIncome(Board* board)
{
    if(ownerId == 0) return 0;
    std::vector<Hexagon*> province = this->neighbours(board, BIG_NUMBER, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    if(province.size() < 2) return 0;

    int total = 0;
    for(Hexagon* h : province)
    {
        total += incomeBoard[(int)h->getResident()] + 1;
    }

    return total;
}

// Tańsza od calculateProvince() ale jedynie znajduje istniejącą prowincję
std::vector<Hexagon*> Hexagon::province(Board* board)
{
    if(ownerId == 0) return std::vector<Hexagon*>{ this };
    std::vector<Hexagon*> neighbours = this->neighbours(board, BIG_NUMBER, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    for(int i = 1; i < neighbours.size(); i++)
    {
        if(castle(neighbours[i]->getResident()))
        {
            Hexagon* t = neighbours[0];
            neighbours[0] = neighbours[i];
            neighbours[i] = t;
            return neighbours;
        }
    }
    return neighbours;
}

// Sprawdza czy dany żołnierz może wejść na inne pole. Nie używać dla innych rezydentów
bool Hexagon::allows(Board* board, Resident warrior, uint8 ownerId)
{
    if(!unmovedWarrior(warrior)) return false;
    if(water(resident)) return false; // nie po wodzie
    if(this->ownerId == ownerId)
    {
        if(::warrior(resident)) return ::warrior(mergeWarriors(resident, warrior)); // mieszanie żołnierzy
        return power(resident) < 0; // żołnierz może deptać po swoim lądzie, drzewach i grobach
    }
    int attackerPower = power(warrior);
    if(attackerPower == 4) return true; // czwarty żołnierz rozjeżdża wszystko (włącznie z innymi czwartymi żołnierzami)
    std::vector<Hexagon*> neighbours = this->neighbours(board, 0, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    for(Hexagon* n : neighbours)
    {
        if(power(n->getResident()) >= attackerPower) return false; // jeśli ta płytka lub jej sąsiad ma kogoś o większej lub równej mocy to nie można wejść
    }
    return true;
}

void addNeighboursLayerWithBorder(Board* board, std::unordered_set<Hexagon*>& visited, std::unordered_set<Hexagon*>& border, std::vector<Hexagon*>& hexagons, int recursion, std::function<bool(Hexagon*)> filter)
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
            if(hex != nullptr && !visited.count(hex) && !border.count(hex))
            {
                if(filter(hex))
                {
                    visited.insert(hex);
                    newHexagons.push_back(hex);
                }
                else
                {
                    border.insert(hex);
                }
            }
        }
    }
    if(recursion > 0) addNeighboursLayerWithBorder(board, visited, border, newHexagons, recursion - 1, filter);
}

void calculateEnvironment(Board* board, Hexagon* center, uint8 oldOwnerId)
{
    if(oldOwnerId != 0) // by ograniczyć wywołania drogiej kalkulacji wybieramy tylko punkty które przypuszczamy że należą do różnych prowincji
    {
        std::vector<Hexagon*> neighboursRequiringCalculation;
        neighboursRequiringCalculation.reserve(6);
        bool add = true;
        bool trimLast = false;
        auto& directions = (center->getX() % 2 == 0) ? evenDirections : oddDirections;
        for(int i = 0; i < directions.size(); i++)
        {
            auto [dx, dy] = directions[i];
            Hexagon* hex = board->getHexagon(center->getX() + dx, center->getY() + dy); // getHexagon() robi sprawdzanie zakresów
            if(hex == nullptr || hex->getOwnerId() == 0 || hex->getOwnerId() != oldOwnerId)
            {
                add = true;
            }
            else if(add)
            {
                if(!(i == directions.size() - 1 && trimLast && neighboursRequiringCalculation.size() > 1)) neighboursRequiringCalculation.push_back(hex);
                add = false;
                if(i == 0) trimLast = true; // jeśli pierwszy i ostatni dotykają się możemy jednego pominąć
            }
        }

        for(Hexagon* h : neighboursRequiringCalculation) 
        {
            h->calculateProvince(board);
        }

        if(board->getCountries()[oldOwnerId].getCastles().size() == 0) board->eliminateCountry(oldOwnerId);
    }

    center->calculateProvince(board); // kalkulacja dla siebie (cel dotyka wszystkich prowincji atakującego dla których terytorium mogłoby się zmienić więc wystarczy wywołać ją tylko dla niego)
}


void Board::eliminateCountry(uint8 id)
{
    leaderboardInsert(id);
    if(leaderboard.size() >= countries.size() - 1) // Jeśli został tylko jeden gracz żywy
    {
        for(uint8 playerId = 1; playerId <= countries.size(); playerId++) // Dodajemy ostatniego żywego
        {
            bool add = true;
            for(int j = 0; j < leaderboard.size(); j++)
            {
                if(leaderboard[j] == playerId)
                {
                    add = false;
                    break;
                }
            }
            if(add) leaderboardInsert(playerId);
        }
        std::cout << "Game over!\nLeaderboard:\n";
        for(int i = 0; i < leaderboard.size(); i++)
        {
            std::cout << i + 1 << ". Player " << (int)leaderboard[i] << '\n';
        }
    }
}

// Zwraca ilość pieniędzy zamku przed usunięciem
int Hexagon::removeCastle(Board* board, bool eliminateCastleless)
{
    if(castle(resident)) resident = Resident::Empty;
    auto& castlesMap = board->getCountry(ownerId)->getCastles();
    if(castlesMap.count(this))
    {
        int money = castlesMap[this];
        castlesMap.erase(this);
        if(eliminateCastleless && !castlesMap.size()) board->eliminateCountry(getOwnerId());
        return money;
    }
    return 0;
}

// Używać dla żołnierzy (kładzionych, nie przesuwanych), farm i wież
std::vector<Hexagon*> Hexagon::possiblePlacements(Board* board, Resident resident)
{
    if(ownerId == 0) return std::vector<Hexagon*>();
    std::vector<Hexagon*> valid;
    if(unmovedWarrior(resident))
    {
        /*return neighbours(board, BIG_NUMBER, true, [this, board, resident](Hexagon* h)
        {
            if(h->ownerId == this->ownerId) return h->allows(board, resident, this->ownerId); // pola prowincji które pozwalają na tego żołnierza
            else return h->allows(board, resident, this->ownerId) && h->neighbours(board, 0, false, [this](Hexagon* h) { return h->ownerId == this->ownerId; }).size(); // obce pola wokół prowincji które pozwalają na tego żołnierza
        });*/
        std::unordered_set<Hexagon*> visited = { this };
        std::unordered_set<Hexagon*> border;
        std::vector<Hexagon*> newHexagons = { this };
        addNeighboursLayerWithBorder(board, visited, border, newHexagons, BIG_NUMBER, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
        
        valid.reserve(visited.size() + border.size());
        for (Hexagon* h : visited) if(h->allows(board, resident, ownerId)) valid.push_back(h);
        for (Hexagon* h : border) if(h->allows(board, resident, ownerId)) valid.push_back(h);
        return valid;
    }
    std::vector<Hexagon*> province = neighbours(board, BIG_NUMBER, true, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    valid.reserve(province.size());
    if(resident == Resident::Farm)
    {
        for (Hexagon* h : province) // nasze pola które graniczą z innymi farmami lub zamkiem
        {
            if((empty(h->resident) || gravestone(h->resident)) && h->neighbours(board, 0, false, [this](Hexagon* h) { return h->ownerId == this->ownerId && (castle(h->resident) || farm(h->resident)); }).size()) valid.push_back(h);
        }
        return valid;
    }
    if(resident == Resident::Tower)
    {
        for (Hexagon* h : province) // nasze puste pola
        {
            if(empty(h->resident) || gravestone(h->resident)) valid.push_back(h);
        }
        return valid;
    }
    if(resident == Resident::StrongTower)
    {
        for (Hexagon* h : province) // nasze puste pola lub zwykłe wieże
        {
            if(empty(h->resident) || gravestone(h->resident) || h->resident == Resident::Tower) valid.push_back(h);
        }
        return valid;
    }
    return std::vector<Hexagon*>();
}

void Hexagon::removeTree(Board *board)
{
    auto& castlesMap = board->getCountry(ownerId)->getCastles();
    std::vector<Hexagon*> province = this->province(board);
    if(castle(province[0]->resident)) castlesMap[province[0]] += 3;
    /*for (auto& it : castlesMap)
    {
        if (std::find(it.first->province(board).begin(),it.first->province(board).end(),this) != it.first->province(board).end())
        {
            it.second+=3;
            break;
        }
    }*/
}

// Kładzie żołnierza, farmę lub wieżę na miejsce. Zwraca czy położenie się powiodło
bool Hexagon::place(Board* board, Resident resident, Hexagon* placement, bool send)
{
    Hexagon* castleHex;
    if(castle(getResident())) castleHex = this;
    else castleHex = province(board)[0];
    if(!castle(castleHex->getResident())) return false;
    int price = castleHex->price(board, resident);
    if(!price) return false;
    auto& castles = board->getCountry(castleHex->getOwnerId())->getCastles();
    if(price > castles[castleHex]) return false;
    
    if(unmovedWarrior(resident))
    {
        if(!placement->allows(board, resident, castleHex->getOwnerId())) return false;
        if(placement->getOwnerId() == castleHex->getOwnerId())
        {
            if(warrior(placement->getResident()))
            {
                Resident merged = mergeWarriors(resident, placement->getResident()); // mieszanie żołnierzy
                if(!warrior(merged)) return false;
                placement->setResident(merged);
            }
            else if(gravestone(placement->getResident()))
            {
                placement->setResident(::move(resident));
            }
            else if(tree(placement->getResident()))
            {
                placement->removeTree(board);
                placement->setResident(::move(resident));

            }
            else
            {
                if(castle(placement->getResident())) placement->removeCastle(board, false);
                placement->setResident(resident);
            }
        }
        else
        {
            uint8 oldOwnerId = placement->getOwnerId();
            if(castle(placement->getResident())) board->getCountry(oldOwnerId)->tempMoneyStorage+=placement->removeCastle(board, false);

            placement->setResident(::move(resident));
            placement->setOwnerId(castleHex->getOwnerId());

            calculateEnvironment(board, placement, oldOwnerId);
        }
    }
    else if(farm(resident))
    {
        placement->setResident(resident);
    }
    else if(tower(resident))
    {
        placement->setResident(resident);
    }
    else return false;
    castles[castleHex] -= price;

    if(send)
    {
        char content[12]; // tag (1) + liczba akcji (1) + tag akcji (1) + rezydent (1) + kordy z (2+2) + kordy do (2+2)
        char* position = content;

        *position++ = ACTION_SOCKET_TAG; // tag
        *position++ = 1; // liczba akcji (jedna)
        *position++ = 1; // tag akcji (1 - położenie)
        *position++ = static_cast<char>(resident); // rezydent

        for(coord c : { x, y, placement->getX(), placement->getY() })
        {
            ucoord net = htons(c);
            memcpy(position, &net, sizeof(net));
            position += sizeof(net);
        }
        sendData(content, sizeof(content), -1);
    }

    return true;
}


// Używać dla żołnierzy (przesuwanych, nie kładzionych)
std::vector<Hexagon*> Hexagon::possibleMovements(Board* board)
{
    if(ownerId == 0) return std::vector<Hexagon*>();
    std::vector<Hexagon*> valid;
    if(warrior(resident))
    {
        std::unordered_set<Hexagon*> visited = { this };
        std::unordered_set<Hexagon*> border;
        std::vector<Hexagon*> newHexagons = { this };
        addNeighboursLayerWithBorder(board, visited, border, newHexagons, 3, [this](Hexagon* h) { return h->ownerId == this->ownerId; });

        valid.reserve(visited.size() + border.size());
        for (Hexagon* h : visited) if(h->allows(board, resident, ownerId)) valid.push_back(h);
        for (Hexagon* h : border) if(h->allows(board, resident, ownerId)) valid.push_back(h);
    }
    return valid;
}

// Przesuwa wojownika na inne miejsce. Zwraca czy przesunięcie się powiodło
bool Hexagon::move(Board* board, Hexagon* destination, bool send)
{
    if(!unmovedWarrior(resident)) return false;
    if(!destination->allows(board, resident, ownerId)) return false;
    uint8 oldOwnerId = destination->getOwnerId();
    if(oldOwnerId == ownerId && warrior(destination->getResident()))
    {
        Resident merged = mergeWarriors(resident, destination->getResident()); // mieszanie żołnierzy
        if(!warrior(merged)) return false;
        destination->setResident(merged);
    }
    else
    {
        if(castle(destination->getResident())) destination->removeCastle(board, false);
        if (tree(destination->getResident())) destination->removeTree(board);
        destination->setResident(::move(resident));
    }
    setResident(Resident::Empty);

    if(ownerId != oldOwnerId)
    {
        destination->setOwnerId(ownerId);
        calculateEnvironment(board, destination, oldOwnerId);
    }

    if(send)
    {
        char content[11]; // tag (1) + liczba akcji (1) + tag akcji (1) + kordy z (2+2) + kordy do (2+2)
        char* position = content;

        *position++ = ACTION_SOCKET_TAG; // tag
        *position++ = 1; // liczba akcji (jedna)
        *position++ = 2; // tag akcji (2 - położenie)

        for(coord c : { x, y, destination->getX(), destination->getY() })
        {
            ucoord net = htons(c);
            memcpy(position, &net, sizeof(net));
            position += sizeof(net);
        }
        sendData(content, sizeof(content), -1);
    }
    return true;
}

Country::Country(std::vector<Hexagon*> castles)
{
    for(Hexagon* h : castles)
    {
        h->setResident(Resident::Castle);
        this->castles[h] = 100;
    }
}

