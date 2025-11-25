#include "board.h"
#include <unordered_set>
#include <cstring>

void initializeSocket()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return;
    }
#endif

#ifdef _WIN32
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
#else
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
#endif
    {
        std::cout << "Failed to create socket\n";
        sock = -1;
        return;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(SOCKET_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0)
    {
        std::cout << "Invalid address\n";
        sock = -1;
        return;
    }

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0)
    {
        std::cout << "Failed to connect to server\n";
        sock = -1;
        return;
    }
}

void closeSocket()
{
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    sock = 0;
}

void markAll(std::vector<Hexagon *> hexagons)
{
    for(Hexagon* h : hexagons) h->mark();
}

void unmarkAll(std::vector<Hexagon*> hexagons)
{
    for(Hexagon* h : hexagons) h->unmark();
}


void sendMagicNumbers()
{
    if (sock == -1)
    {
        std::cout << "Socket not initialized, cannot send magic numbers\n";
        return;
    }
    char magicNumbers[] = SOCKET_MAGIC_NUMBERS;
    char content[1 + sizeof(magicNumbers)];
    content[0] = MAGIC_SOCKET_TAG;
    memcpy(content + 1, magicNumbers, sizeof(magicNumbers));
    int sentBytes = 0;
    while (sentBytes < sizeof(content))
    {
        int r = send(sock, content + sentBytes, sizeof(content) - sentBytes, 0);
        if (r <= 0)
        {
            std::cout << "Failed to send magic numbers\n";
            break;
        }
        sentBytes += r;
    }
}

void sendConfirmation(bool approved, bool awaiting)
{
    if (sock == -1)
    {
        std::cout << "Socket not initialized, cannot send confirmation\n";
        return;
    }
    char content[3];
    content[0] = CONFIRMATION_SOCKET_TAG;
    content[1] = approved;
    content[2] = awaiting;
    int sentBytes = 0;
    while (sentBytes < sizeof(content))
    {
        int r = send(sock, content + sentBytes, sizeof(content) - sentBytes, 0);
        if (r <= 0)
        {
            std::cout << "Failed to send confirmation\n";
            break;
        }
        sentBytes += r;
    }
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


void Board::InitializeRandomA(int min, int max)
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

void Board::InitializeCountriesA(uint8 countriesCount, int minCountrySize, int maxCountrySize)
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
}

void Board::socketSend()
{
    if(sock == -1)
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

    int sentBytes = 0;
    while (sentBytes < total)
    {
        int r = send(sock, reinterpret_cast<char*>(result) + sentBytes, total - sentBytes, 0);
        if (r <= 0)
        {
            std::cout << "Failed to send board data\n";
            break;
        }
        sentBytes += r;
    }
    delete[] result;
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

// Sam liczy farmy
void Hexagon::setCastle(Board* board, int money)
{
    auto castlesMap = board->getCountry(ownerId)->getCastles();
    resident = Resident::Castle;

    auto province = neighbours(board, BIG_NUMBER, false, [this](Hexagon* h) { return h->ownerId == this->ownerId; });
    int farmsNumber = 0;
    for(Hexagon* h : province)
    {
        if(farm(h->getResident())) farmsNumber++;
    }

    castlesMap[this] = MoneyAndFarms({money, farmsNumber});
}

void Hexagon::setCastle(Board* board, int money, int farms)
{
    auto castlesMap = board->getCountry(ownerId)->getCastles();
    resident = Resident::Castle;
    castlesMap[this] = MoneyAndFarms({money, farms});
}

int Hexagon::removeCastle(Board* board)
{
    if(castle(resident)) resident = Resident::Empty;
    auto castlesMap = board->getCountry(ownerId)->getCastles();
    if(castlesMap.count(this))
    {
        int money = castlesMap[this].money;
        castlesMap.erase(this);
        return money;
    }
    return 0;
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
        std::unordered_map<Hexagon*, MoneyAndFarms> castlesMap = board->getCountry(ownerId)->getCastles();
        int mostFarms = 0;
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
        }
        for(int i = 1; i < castlesNumber; i++)
        {
            castlesMap[province[0]].money += province[i]->removeCastle(board);
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
        newCastle->setCastle(board, 0);
    }
    return province;
}

// Tańsza od calculateProvince() ale jedynie znajduje istniejącą prowincję. Używać do pozyskania prowincji która na pewno jest poprawna
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
    if(!::warrior(warrior)) return false;
    if(water(resident)) return false; // nie po wodzie
    if(this->ownerId == ownerId)
    {
        if(::warrior(resident)) return power(resident) + power(warrior) <= 4; // mieszanie żołnierzy
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

// Używać dla żołnierzy, farm i wież
std::vector<Hexagon*> Hexagon::possiblePlacements(Board* board, Resident resident)
{
    if(ownerId == 0) return std::vector<Hexagon*>();
    std::vector<Hexagon*> valid;
    if(warrior(resident))
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
bool Hexagon::move(Board* board, Hexagon* destination)
{
    if(!warrior(resident)) return false;
    if(!destination->allows(board, resident, ownerId)) return false;
    uint8 oldOwnerId = destination->getOwnerId();
    if(oldOwnerId == ownerId && warrior(destination->getResident()))
    {
        int powerSum = power(resident) + power(destination->getResident()); // mieszanie żołnierzy
        if(powerSum < 1 || powerSum > 4) return false;
        destination->setResident((Resident)(powerSum + (int)Resident::Warrior1 - 1));
    }
    else
    {
        if(castle(destination->getResident())) destination->removeCastle(board);
        destination->setResident(resident);
    }
    setResident(Resident::Empty);

    if(ownerId != oldOwnerId)
    {
        destination->setOwnerId(ownerId);
        if(oldOwnerId != 0) // by ograniczyć wywołania drogiej kalkulacji wybieramy tylko punkty które przypuszczamy że należą do różnych prowincji
        {
            std::vector<Hexagon*> neighboursRequiringCalculation;
            bool add = true;
            bool trimLast = false;
            auto& directions = (destination->x % 2 == 0) ? evenDirections : oddDirections;
            for(int i = 0; i < directions.size(); i++)
            {
                auto [dx, dy] = directions[i];
                Hexagon* hex = board->getHexagon(destination->x + dx, destination->y + dy); // getHexagon() robi sprawdzanie zakresów
                if(hex == nullptr || hex->ownerId == 0 || hex->ownerId == destination->ownerId)
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
                //std::cout << "Kalkulacja dla " << (int)h->getOwnerId() << "\n";
            }
        }

        destination->calculateProvince(board); // kalkulacja dla siebie (cel dotyka wszystkich prowincji atakującego dla których terytorium mogłoby się zmienić więc wystarczy wywołać ją tylko dla niego)
    }
    return true;
}

// Konstruktor nie liczy farm, jeśli Ci tego trzeba użyj setCastle(Board* board, int money) na tych heksagonach później
Country::Country(std::vector<Hexagon*> castles)
{
    for(Hexagon* h : castles)
    {
        h->setResident(Resident::Castle);
        this->castles[h] = MoneyAndFarms({10, 0});
    }
}

Player::Player(Country* country, unsigned int maxMoveTime) : country(country), maxMoveTime(maxMoveTime)
{
    country->setPlayer(this);
}

LocalPlayer::LocalPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

BotPlayer::BotPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}
