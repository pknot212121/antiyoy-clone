#include "game.h"

#include <set>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "resource_manager.h"


// Game-related State data
SpriteRenderer* Renderer;


GameConfigData::GameConfigData(coord x, coord y, int seed, std::string playerMarkers) : x(x), y(y), seed(seed), playerMarkers(playerMarkers)
{

}

void GameConfigData::sendGameConfigData(int receivingSocket)
{
    if(invalidSocks())
    {
        std::cout << "Socket not initialized, cannot send game config data\n";
        return;
    }
    int total = 1 + estimateSize();
    char* content = new char[total];
    content[0] = CONFIGURATION_SOCKET_TAG;
    char* position = content + 1;
    ucoord xNet = htons(x);
    memcpy(position, &xNet, sizeof(xNet));
    position += sizeof(xNet);
    ucoord yNet = htons(y);
    memcpy(position, &yNet, sizeof(yNet));
    position += sizeof(yNet);
    unsigned int seedNet = htonl(seed);
    memcpy(position, &seedNet, sizeof(seedNet));
    position += sizeof(seedNet);
    *((uint8*)position) = playerMarkers.length();
    position++;
    memcpy(position, playerMarkers.data(), playerMarkers.length());

    sendData(receivingSocket, content, total);
    
    delete[] content;
}

bool GameConfigData::receiveFromSocket(int deliveringSocket)
{
    if (deliveringSocket < 0) return false;

    char tag;
    int r = recv(deliveringSocket, &tag, 1, 0);
    if (r <= 0 || tag != CONFIGURATION_SOCKET_TAG) return false;

    auto recvAll = [&](char* buf, int size)
    {
        int total = 0;
        while (total < size)
        {
            int r = recv(deliveringSocket, buf + total, size - total, 0);
            if (r <= 0) return false;
            total += r;
        }
        return true;
    };

    ucoord xNet, yNet;
    uint32_t seedNet;
    uint8_t markerLen;

    if (!recvAll((char*)&xNet, sizeof(xNet))) return false;
    if (!recvAll((char*)&yNet, sizeof(yNet))) return false;
    if (!recvAll((char*)&seedNet, sizeof(seedNet))) return false;
    if (!recvAll((char*)&markerLen, sizeof(markerLen))) return false;

    playerMarkers.resize(markerLen);
    if (markerLen > 0)
    {
        if (!recvAll(playerMarkers.data(), markerLen)) return false;
    }

    x = ntohs(xNet);
    y = ntohs(yNet);
    seed = ntohl(seedNet);

    return true;
}


Game::Game(unsigned int width, unsigned int height)
    : State(GameState::GAME_ACTIVE), Width(width), Height(height), board() {
}

Game::~Game()
{
    for(Player* p : players)
    {
        delete p;
    }
    delete Renderer;
}

void Game::Init(coord x, coord y, int seed, std::string playerMarkers, std::vector<int> maxMoveTimes)
{
    // load shaders
    ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.fs", nullptr, "sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width), 
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
    // load textures
    ResourceManager::LoadTexture("textures/soilder1_256.png",true,"soilder1");
    ResourceManager::LoadTexture("textures/soldier2_256.png",true,"soilder2");
    ResourceManager::LoadTexture("textures/hexagon.png", true, "hexagon");
    ResourceManager::LoadTexture("textures/placeholder.png",true,"placeholder");
    ResourceManager::LoadTexture("textures/exclamation.png",true,"exclamation");
    ResourceManager::LoadTexture("textures/castle_256.png",true,"castle");
    ResourceManager::LoadTexture("textures/tree_placeholder.png",true,"tree_placeholder");

    Text = new TextRenderer(this->Width, this->Height);
    Text->Load("Roboto-Black.ttf", 24);
    gen = std::mt19937(seed == 0 ? std::random_device{}() : seed);

    //coord x = 10;
    //coord y = 10;
    board = new Board(x, y, this);
    int total = x * y;
    board->InitializeRandomA(total * 0.5, total * 0.9);
    board->InitializeCountriesA(playerMarkers.length(), 6, 8);

    auto countries = board->getCountries();
    if(countries.size() == playerMarkers.length())
    {
        players.reserve(playerMarkers.length());
        for(uint8 i = 0; i < playerMarkers.length(); i++)
        {
            if(playerMarkers[i] == 'L') players.push_back(new LocalPlayer(&countries[i], i+1, this, maxMoveTimes[i])); // rzutowanie maxMoveTimes[i] na unsigned int zmienia -1 na max unsigned int
            else if(playerMarkers[i] == 'B') players.push_back(new BotPlayer(&countries[i], i+1, maxMoveTimes[i]));
            else
            {
                std::cout << "Unidentified player markers\n";
                getchar();
                std::exit(1);
            }
        }
        /*for(Country& country : countries)
        {
            players.emplace_back(&country);
        }*/
    }
    else
    {
        std::cout << "Countries initialization error\n";
        getchar();
        std::exit(1);
    }

}

void Game::Update(float dt)
{
    // getPlayer odejmuje od podanego indeksu 1 co jest kluczowe (id graczy numerowane są od 1), analogiczna do getCountry()
    getPlayer(board->getCurrentPlayerId())->act();
}

void Game::Resize(int width, int height)
{
    this->Width = width;
    this->Height = height;
    Text->TextShader.SetMatrix4("projection", glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f), true);
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);

    ResourceManager::GetShader("sprite").Use().SetMatrix4("projection", projection);
}


void Game::ProcessInput(float dt)
{
    if (this->scroll != 0)
    {
        float zoomFactor = (this->scroll == 1) ? 1.1f : 0.9f;
        float centerX = this->Width / 2.0f;
        float centerY = this->Height / 2.0f;

        Renderer->Zoom(zoomFactor, centerX, centerY);

        scroll = 0;
    }
    if (clickedMovingKeys[GLFW_KEY_W])
    {
        Renderer -> addToDisplacementY(10);
    }
    if (clickedMovingKeys[GLFW_KEY_A])
    {
        Renderer ->addToDisplacementX(10);
    }
    if (clickedMovingKeys[GLFW_KEY_S])
    {
        Renderer -> addToDisplacementY(-10);
    }
    if (clickedMovingKeys[GLFW_KEY_D])
    {
        Renderer -> addToDisplacementX(-10);
    }
}

int Game::GetSelectedCastleReserves()
{
    std::unordered_map<Hexagon*, int>& m= board->getCountry(board->getCurrentPlayerId())->getCastles();
    int sum=0;
    if (provinceSelector!=nullptr)
    {
        for (auto a : m)
        {
            if (provinceSelector->province(board)[0]==a.first)
            {
                sum=a.second;
                break;
            }
        }
    }
    return sum;
}

int Game::GetSelectedCastleIncome()
{
    int sum=0;
    if (provinceSelector!=nullptr)
    {
        sum=provinceSelector->province(board)[0]->calculateProvinceIncome(board);
    }
    return sum;
}


void Game::Render()
{
    Renderer -> DrawBoard(board, this->Width, this->Height,board->getCurrentPlayerId());

    Text->RenderText("Money:"+std::to_string(GetSelectedCastleReserves()) ,10.0f, 10.0f, 1.0f);
    Text->RenderText("Income:"+std::to_string(GetSelectedCastleIncome()),this->Width/2,10.0f,1.0f);
}


void Board::nextTurn() // Definicja przeniesiona tutaj ze względu na game->getPlayer()
{
    std::unordered_map<Hexagon*, int>& oldCastles = getCountry(currentPlayerId)->getCastles();
    for (auto& [caslteHex, money] : oldCastles)
    {
        std::vector<Hexagon*> province = caslteHex->neighbours(this, BIG_NUMBER, true, [caslteHex](Hexagon* h) { return h->getOwnerId() == caslteHex->getOwnerId(); });
        for(Hexagon* h : province)
        {
            if(unmovedWarrior(h->getResident())) h->setResident(move(h->getResident()));
        }

    }

    uint8 oldId = currentPlayerId;

    bool retry = true;
    while(retry) // Szukamy gracza który jeszcze nie jest na tablicy wyników (jeszcze żyje)
    {
        currentPlayerId = currentPlayerId % countries.size() + 1;
        retry = false;
        for(uint8 id : leaderboard)
        {
            if(currentPlayerId == id) retry = true;
        }
    }

    std::unordered_map<Hexagon*, int>& castles = getCountry(currentPlayerId)->getCastles();
    for (auto& [caslteHex, money] : castles)
    {
        std::vector<Hexagon*> province = caslteHex->neighbours(this, BIG_NUMBER, true, [caslteHex](Hexagon* h) { return h->getOwnerId() == caslteHex->getOwnerId(); });
        for(Hexagon* h : province)
        {
            if(movedWarrior(h->getResident())) h->setResident(unmove(h->getResident()));
            if (h->getResident()==Resident::Gravestone) h->setResident(Resident::PineTree);
        }
        money += calculateIncome(province);

        // Kill them if money is low, probably at your turn's end
        if (money<0)
        {
            for (Hexagon *h : province)
            {
                if (h->getResident()==Resident::Warrior1 || h->getResident()==Resident::Warrior2 || h->getResident()==Resident::Warrior3 || h->getResident()==Resident::Warrior4)
                {
                    h->setResident(Resident::Gravestone);
                }
            }
        }

    }

    //game->getPlayer(currentPlayerId)->actStart();
}


Player::Player(Country* country, uint8 id, unsigned int maxMoveTime) : country(country), id(id), maxMoveTime(maxMoveTime)
{
    country->setPlayer(this);
}

LocalPlayer::LocalPlayer(Country* country, uint8 id, Game* game, unsigned int maxMoveTime) : Player(country, id, maxMoveTime), game(game)
{
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

void LocalPlayer::act()
{

    if (game->State == GameState::GAME_ACTIVE)
    {
        if (!game->isFirstProvinceSet)
        {
            std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
            this->game->provinceSelector = *hexes.begin();
            game->isFirstProvinceSet = true;
        }

        if(keysToResidents.contains(game->pressedKey) && game->provinceSelector!=nullptr)
        {
            std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
            std::vector<Hexagon*> neigh = game->provinceSelector->possiblePlacements(game->board,keysToResidents[game->pressedKey]);
            Renderer -> setBrightenedHexes(neigh);
        }
        if (game->mousePressed)
        {
            float size = game->Width / game->board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * game->board->getWidth();
            Point p = Renderer -> CheckWhichHexagon(game->cursorPosX,game->cursorPosY,size/2);
            Hexagon *hex = game->board->getHexagon(p.x,p.y);
            if (p.x<game->board->getWidth() && p.x>=0 && p.y<game->board->getHeight() && p.y>=0)
            {

                if(keysToResidents.contains(game->pressedKey) && !game->isHexSelected){
                    spawnAction(hex,p);
                }
                else
                {
                    moveAction(hex,p);
                    SelectAction(hex,p);
                }
                game-> mousePressed = false;
            }


        }
        if (!keysToResidents.contains(game->pressedKey) && game->isHexSelected==false && game->provinceSelector!=nullptr)
        {
            Renderer->ClearBrightenedHexes();
        }

        if(game->pressedKey==GLFW_KEY_ENTER)
        {
            game->enterPressed = true;

        }
        if(game->pressedKey!=GLFW_KEY_ENTER && game->enterPressed)
        {
            game->enterPressed = false;
            game->isFirstProvinceSet = false;
            game->board->nextTurn();

        }
    }

}

void LocalPlayer::moveAction(Hexagon* hex,Point p)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
    Resident res = game->board->getHexagon(p.x,p.y)->getResident();
    if (game->isHexSelected){
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        if (auto it = std::ranges::find(nearby,hex);it!=nearby.end()){
            if (game->selectedHex!=hex)
                game->selectedHex->move(game->board,hex);
        }
        Renderer -> ClearBrightenedHexes();
        game->isHexSelected=false;
    }
    else if ((res==Resident::Warrior1 || res==Resident::Warrior2 || res==Resident::Warrior3 || res==Resident::Warrior4) && hexes.contains(hex))
    {
        game->selectedHex=hex;game->isHexSelected=true;
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        Renderer -> setBrightenedHexes(nearby);
    }

}

void LocalPlayer::spawnAction(Hexagon* hex,Point p)
{
    if (game->provinceSelector!=nullptr)
    {
        std::vector<Hexagon*> neigh = game->provinceSelector->possiblePlacements(game->board,keysToResidents[game->pressedKey]);
        if (auto it = std::ranges::find(neigh,hex); it!=neigh.end()){
            game->provinceSelector->place(game->board,keysToResidents[game->pressedKey],hex);
        }
        Renderer -> ClearBrightenedHexes();
    }

}

void LocalPlayer::SelectAction(Hexagon *hex,Point p)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
    if (hexes.contains(hex))
    {
        game->provinceSelector = hex;
    }
}

BotPlayer::BotPlayer(Country* country, uint8 id, int receiveSock, unsigned int maxMoveTime) : Player(country, id, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}

/*void BotPlayer::actStart()
{
    sendTurnChange(id);
}*/

void BotPlayer::act()
{
    
}

NetworkPlayer::NetworkPlayer(Country* country, uint8 id, int receiveSock, unsigned int maxMoveTime) : Player(country, id, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Network player created with max move time " << maxMoveTime << "\n";
}

void NetworkPlayer::act()
{
}
