#include "game.h"

#include <set>
#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "resource_manager.h"


// Game-related State data


std::mt19937 GameConfigData::gcdGen(std::random_device{}() + 12345);

GameConfigData::GameConfigData(coord x, coord y, unsigned int seed, int minProvinceSize, int maxProvinceSize, std::string playerMarkers, std::vector<int> maxMoveTimes)
: x(x), y(y), seed(seed), minProvinceSize(minProvinceSize), maxProvinceSize(maxProvinceSize), playerMarkers(playerMarkers), maxMoveTimes(maxMoveTimes)
{

}

void GameConfigData::fill(float minLandArea)
{
    int tries = 0;
restart:
    tries++;
    if(x == 0 && y == 0)
    {
        coord min = 5;
        coord max = 25;
        std::uniform_int_distribution<> dist(min, max);
        x = std::uniform_int_distribution<>(min, max)(gcdGen);
        y = std::uniform_int_distribution<>(min > x/3 ? min : x/3, max < x*3 ? max : x*3)(gcdGen);
    }
    else if(x == 0)
    {
        coord min = y / 3;
        coord max = y * 3;
        x = std::uniform_int_distribution<>(min > 4 ? min : 4, max)(gcdGen);
    }
    else if(y == 0)
    {
        coord min = x / 3;
        coord max = x * 3;
        y = std::uniform_int_distribution<>(min > 4 ? min : 4, max)(gcdGen);
    }
    if(!playerMarkers.length())
    {
        std::cout << "Invalid player markers\n";
        getchar();
        std::exit(1);
    }
    if(playerMarkers.length() == 1)
    {
        playerMarkers.assign(std::uniform_int_distribution<>(2, 8)(gcdGen), playerMarkers[0]);
        maxMoveTimes.assign(playerMarkers.length(), maxMoveTimes[0]);
    }
    int maxMaxProvinceSize = x * y * minLandArea / playerMarkers.length();
    if(maxMaxProvinceSize < 2)
    {
        if(tries >= 100)
        {
            std::cout << "Too many failed GCD fills, aborted\n";
            getchar();
            std::exit(1);
        }
        goto restart;
    }
    if(maxProvinceSize < 2 || maxProvinceSize > maxMaxProvinceSize) maxProvinceSize = std::uniform_int_distribution<>(2, maxMaxProvinceSize)(gcdGen);
    if(minProvinceSize < 2 || minProvinceSize > maxProvinceSize) minProvinceSize = std::uniform_int_distribution<>(maxProvinceSize/2 > 2 ? maxProvinceSize/2 : 2, maxProvinceSize)(gcdGen);

    if(!seed) seed = gcdGen();
}

void GameConfigData::randomize(char marker, int maxMoveTime)
{
    coord min = 5;
    coord max = 25;
    std::uniform_int_distribution<> dist(min, max);
restart:
    x = dist(gcdGen);
    y = 0;
    while(y < x/3 || y > x*3) y = dist(gcdGen);
    playerMarkers.assign(std::uniform_int_distribution<>(2, 8)(gcdGen), marker); // od 2 do 8 botów
    maxMoveTimes.assign(playerMarkers.length(), maxMoveTime);
    int maxMaxProvinceSize = x * y / 2 / playerMarkers.length();
    if(maxMaxProvinceSize < 2) goto restart;
    maxProvinceSize = std::uniform_int_distribution<>(2, maxMaxProvinceSize)(gcdGen);
    std::uniform_int_distribution<> minDist(2, maxProvinceSize);
    minProvinceSize = 0;
    while(minProvinceSize < maxProvinceSize/2) minProvinceSize = minDist(gcdGen);
    seed = 0;
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
    char* position = content;

    // Tag
    *position++ = CONFIGURATION_SOCKET_TAG;

    // x, y
    ucoord xNet = htons(x);
    memcpy(position, &xNet, sizeof(xNet));
    position += sizeof(xNet);

    ucoord yNet = htons(y);
    memcpy(position, &yNet, sizeof(yNet));
    position += sizeof(yNet);

    // seed
    uint32_t seedNet = htonl(seed);
    memcpy(position, &seedNet, sizeof(seedNet));
    position += sizeof(seedNet);

    // minProvinceSize, maxProvinceSize
    uint32_t minNet = htonl(minProvinceSize);
    memcpy(position, &minNet, sizeof(minNet));
    position += sizeof(minNet);

    uint32_t maxNet = htonl(maxProvinceSize);
    memcpy(position, &maxNet, sizeof(maxNet));
    position += sizeof(maxNet);

    // playerMarkers
    *position++ = (uint8_t)playerMarkers.size();
    memcpy(position, playerMarkers.data(), playerMarkers.size());
    position += playerMarkers.size();

    // maxMoveTimes
    *position++ = (uint8_t)maxMoveTimes.size();
    for (int v : maxMoveTimes)
    {
        uint32_t vNet = htonl(v);
        memcpy(position, &vNet, sizeof(vNet));
        position += sizeof(vNet);
    }

    sendData(content, total, receivingSocket);

    delete[] content;
}

bool GameConfigData::receiveFromSocket(int deliveringSocket, bool tag)
{
    if (invalidSocks())
    {
        std::cout << "Socket not initialized, cannot receive magic numbers data\n";
        return false;
    }
    if (deliveringSocket < 0) return false;

    if(tag)
    {
        char tag;
        if (recv(deliveringSocket, &tag, 1, 0) <= 0 || tag != CONFIGURATION_SOCKET_TAG) return false;
    }

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
    uint32_t minNet, maxNet;

    uint8_t markerLen;
    uint8_t moveTimesLen;

    if (!recvAll((char*)&xNet, sizeof(xNet))) return false;
    if (!recvAll((char*)&yNet, sizeof(yNet))) return false;
    if (!recvAll((char*)&seedNet, sizeof(seedNet))) return false;
    if (!recvAll((char*)&minNet, sizeof(minNet))) return false;
    if (!recvAll((char*)&maxNet, sizeof(maxNet))) return false;

    if (!recvAll((char*)&markerLen, sizeof(markerLen))) return false;

    playerMarkers.resize(markerLen);
    if (markerLen > 0)
    {
        if (!recvAll(playerMarkers.data(), markerLen))
            return false;
    }

    if (!recvAll((char*)&moveTimesLen, sizeof(moveTimesLen))) return false;

    maxMoveTimes.resize(moveTimesLen);
    for (int i = 0; i < moveTimesLen; i++)
    {
        uint32_t vNet;
        if (!recvAll((char*)&vNet, sizeof(vNet))) return false;
        maxMoveTimes[i] = ntohl(vNet);
    }

    x = ntohs(xNet);
    y = ntohs(yNet);
    seed = ntohl(seedNet);
    minProvinceSize = ntohl(minNet);
    maxProvinceSize = ntohl(maxNet);

    return true;
}



Game::Game(unsigned int width, unsigned int height) : State(GameState::GAME_ACTIVE), Width(width), Height(height), board()
{
}

Game::~Game()
{
    for(Player* p : players)
    {
        delete p;
    }
    delete board;
    delete Renderer;
    delete Text;
}

void Game::LoadResources()
{
    // load shaders
    ResourceManager::LoadShader("sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width), static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    // set render-specific controls

    // load textures
    ResourceManager::loadStaticTextures();
    Text = new TextRenderer(this->Width, this->Height);
    Text->Load(24);
}

void Game::Init(GameConfigData gcd)
{
    float minLandArea = 0.3;
    float maxLandArea = 0.6;

    bool isHost = clientSocks.size() > 0;
    if(isHost || invalidSocks())
    {
        gcd.fill(minLandArea);
    }
    else if(!invalidSocks())
    {
        std::cout << "Awaiting configuration data...\n";
        if(!gcd.receiveFromSocket(sock, true))
        {
            std::cout << "Configuration failed\n";
            getchar();
            closeSockets();
            std::exit(1);
        }
        std::cout << "Successfully configured!\n";
    }

    std::cout << "X: " << (int)gcd.x << ", Y: " << (int)gcd.y << ", Seed: " << gcd.seed << ", Min province: " << gcd.minProvinceSize << ", Max province: " << gcd.maxProvinceSize << '\n';


    std::string markers = gcd.playerMarkers;
    uint8 playersNumber = markers.length();

    board = new Board(gcd.x, gcd.y, gcd.seed, this);
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"), board);
    int total = gcd.x * gcd.y;
    if(gcd.x >= 10 && gcd.y >= 10) board->InitializeRandomWithAnts(5, total * minLandArea, total * maxLandArea);
    else board->InitializeRandom(total * minLandArea, total * maxLandArea);
    board->InitializeCountries(playersNumber, gcd.minProvinceSize, gcd.maxProvinceSize);
    board->spawnTrees(0.2);

    Renderer->getActualDimensions(board);
    Renderer->width = Width;
    Renderer->height = Height;
    Renderer->size = Renderer->getSize(board);


    auto countries = board->getCountries();
    if(countries.size() == playersNumber)
    {
        players.reserve(playersNumber);
        //bool firstBot = false;
        for(uint8 i = 0; i < playersNumber; i++)
        {
            if(markers[i] == 'B')
            {
                gcd.sendGameConfigData(clientSocks[0]); // Pierwszy socket należy do botów
                break;
            }
        }

        uint8 networkSockIndex = 1;
        gcd.playerMarkers.assign(playersNumber, 'N');

        for(uint8 i = 0; i < playersNumber; i++)
        {
            if(markers[i] == 'L') players.push_back(new LocalPlayer(&countries[i], i+1, this, gcd.maxMoveTimes[i])); // rzutowanie maxMoveTimes[i] na unsigned int zmienia -1 na max unsigned int
            else if(markers[i] == 'B') players.push_back(new BotPlayer(&countries[i], i+1, this, clientSocks[0], gcd.maxMoveTimes[i]));
            else if(markers[i] == 'N')
            {
                if(isHost)
                {
                    players.push_back(new NetworkPlayer(&countries[i], i+1, this, clientSocks[networkSockIndex], gcd.maxMoveTimes[i]));
                    gcd.playerMarkers[i] = 'L';
                    gcd.sendGameConfigData(clientSocks[networkSockIndex]);
                    gcd.playerMarkers[i] = 'N';
                    networkSockIndex++;
                }
                else players.push_back(new NetworkPlayer(&countries[i], i+1, this, sock, gcd.maxMoveTimes[i]));
            }
            else
            {
                std::cout << "Unidentified player markers\n";
                getchar();
                std::exit(1);
            }
        }
        gcd.playerMarkers = markers;

        getPlayer(this->board->getCurrentPlayerId())->actStart();
    }
    else
    {
        std::cout << "Countries initialization error\n";
        getchar();
        std::exit(1);
    }

    //this->gcd = gcd;
    std::cout << "Finished init\n";
}

void Game::Restart(GameConfigData& gcd)
{
    provinceSelector=nullptr;
    for (Player* p : players) delete p;
    players.clear();

    delete board;
    board = nullptr;

    delete Renderer;
    Renderer = nullptr;

    Init(gcd);
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
    Renderer->width =width;
    Renderer->height = height;
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

        Renderer->Zoom(zoomFactor, centerX, centerY,board);

        scroll = 0;
    }

    float velocityX = dt*600;
    float velocityY = dt*600;
    if (clickedMovingKeys[GLFW_KEY_W])
    {
        Renderer -> addToDisplacementY(board,velocityY);
    }
    if (clickedMovingKeys[GLFW_KEY_A])
    {
        Renderer ->addToDisplacementX(board,velocityX);
    }
    if (clickedMovingKeys[GLFW_KEY_S])
    {
        Renderer -> addToDisplacementY(board,-velocityY);
    }
    if (clickedMovingKeys[GLFW_KEY_D])
    {
        Renderer -> addToDisplacementX(board,-velocityX);
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
    Renderer -> DrawBoard(board, this->Width, this->Height);
    if (provinceSelector!=nullptr)
    {
        Text->RenderText("Money:"+std::to_string(GetSelectedCastleReserves()) ,10.0f, 10.0f, 1.0f);
        Text->RenderText("Income:"+std::to_string(GetSelectedCastleIncome()),this->Width/2,10.0f,1.0f);
        Text->RenderText("Press R to return to the center",10.0f,this->Height-30.0f,1.0f);
    }
}


Player::Player(Country* country, uint8 id, Game* game, unsigned int maxMoveTime) : country(country), id(id), game(game), maxMoveTime(maxMoveTime)
{
    country->setPlayer(this);
}

void Player::actStart()
{
}

LocalPlayer::LocalPlayer(Country* country, uint8 id, Game* game, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime)
{
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

void LocalPlayer::actStart()
{
    turnEndTime = glfwGetTime() + maxMoveTime;
}

void LocalPlayer::act()
{
    if((game->pressedKey!=GLFW_KEY_ENTER && game->enterPressed) || glfwGetTime()>turnEndTime)
    {
        game->enterPressed = false;
        game->isFirstProvinceSet = false;
        game->provinceSelector=nullptr;
        game->Renderer->shieldHexes.clear();
        game->Renderer->brightenedHexes.clear();
        game->board->nextTurn(true);
        return;
    }
    if (!game->isFirstProvinceSet)
    {
        game->Renderer->setPosToCastle(game->board,id);
        std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
        this->game->provinceSelector = *hexes.begin();
        game->isFirstProvinceSet = true;
    }

    if(keysToResidents.contains(game->pressedKey) && game->provinceSelector!=nullptr)
    {
        std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
        std::vector<Hexagon*> neigh = game->provinceSelector->possiblePlacements(game->board,keysToResidents[game->pressedKey]);
        game->Renderer -> setBrightenedHexes(neigh);
    }
    if (game->mousePressed)
    {
        float size = game->Renderer -> getSize(game->board);
        glm::ivec2 p = game->Renderer -> CheckWhichHexagon(game->cursorPosX,game->cursorPosY,size/2);
        Hexagon *hex = game->board->getHexagon(p.x,p.y);
        if (hex!=nullptr)
        {
            if(keysToResidents.contains(game->pressedKey) && !game->isHexSelected){
                spawnAction(hex,p);
            }
            else
            {
                moveAction(hex,p);
                SelectAction(hex);
            }
            if (tower(hex->getResident()) || castle(hex->getResident())) game->Renderer->shieldHexes=hex->getAllProtectedAreas(game->board);
            else game->Renderer->shieldHexes.clear();

            game-> mousePressed = false;
        }
        else
        {
            game->mousePressed=false;
            game->provinceSelector=nullptr;

        }


    }
    if (!keysToResidents.contains(game->pressedKey) && game->isHexSelected==false && game->provinceSelector!=nullptr)
    {
        game->Renderer->ClearBrightenedHexes();
    }
    if (game->pressedKey==GLFW_KEY_R)
    {
        game->rPressed=true;
    }

        if (game->pressedKey!=GLFW_KEY_R && game->rPressed)
        {
            game->Renderer->setPosToCastle(game->board,id);
            game->rPressed=false;
        }

    if(game->pressedKey==GLFW_KEY_ENTER)
    {
        game->enterPressed = true;

    }
}

void LocalPlayer::moveAction(Hexagon* hex,glm::ivec2 p)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
    Resident res = game->board->getHexagon(p.x,p.y)->getResident();
    if (game->isHexSelected){
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        if (auto it = std::find(nearby.begin(), nearby.end(), hex); it != nearby.end()){
            if (game->selectedHex!=hex)
                game->selectedHex->move(game->board,hex,true);
        }
        game->Renderer -> ClearBrightenedHexes();
        game->isHexSelected=false;
    }
    else if ((res==Resident::Warrior1 || res==Resident::Warrior2 || res==Resident::Warrior3 || res==Resident::Warrior4) && hexes.contains(hex))
    {
        game->selectedHex=hex;game->isHexSelected=true;
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        game->Renderer -> setBrightenedHexes(nearby);
    }
}

void LocalPlayer::spawnAction(Hexagon* hex,glm::ivec2 p)
{
    if (game->provinceSelector!=nullptr)
    {
        std::vector<Hexagon*> neigh = game->provinceSelector->possiblePlacements(game->board,keysToResidents[game->pressedKey]);
        if (auto it = std::find(neigh.begin(), neigh.end(), hex); it != neigh.end()){
            game->provinceSelector->place(game->board,keysToResidents[game->pressedKey],hex,true);
        }
        game->Renderer -> ClearBrightenedHexes();

    }

}

void LocalPlayer::SelectAction(Hexagon *hex)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
    if (hexes.contains(hex))
    {
        game->provinceSelector = hex;
    }
    else
    {
        game->provinceSelector = nullptr;
    }
}

bool receiveActions(int receiveSock, std::vector<char>& output)
{
    switchSocketMode(receiveSock, 0);
    uint8 actionsNumber;
    if(recv(receiveSock, reinterpret_cast<char*>(&actionsNumber), 1, 0) > 0)
    {
        output = {ACTION_SOCKET_TAG, static_cast<char>(actionsNumber)};
        for(int i = 0; i < actionsNumber; i++)
        {
            char action;
            if(recv(receiveSock, &action, 1, 0) > 0)
            {
                output.push_back(action);
                if(action == 0)
                {
                    // nic xd
                }
                else if(action == 1)
                {
                    char content[9];
                    int total = 0;
                    while (total < sizeof(content))
                    {
                        int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                        if (r <= 0) return false;
                        total += r;
                    }
                    output.insert(output.end(), content, content + sizeof(content));
                }
                else if(action == 2)
                {
                    char content[8];
                    int total = 0;
                    while (total < sizeof(content))
                    {
                        int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                        if (r <= 0) return false;
                        total += r;
                    }
                    output.insert(output.end(), content, content + sizeof(content));
                }
                else return false;
            }
            else return false;
        }
    }
    else return false;
    return true;
}

bool canExecuteActions(Board* board, uint8 playerId, char* actions, uint8 actionsNumber, bool* endsTurn = nullptr, bool* endsGame = nullptr)
{
    //std::cout << "canExecuteActions called\n";
    if(playerId != board->getCurrentPlayerId()) return false;
    Board dummyBoard = board->dummy();
    for(int i = 0; i < actionsNumber; i++)
    {
        if(*actions == 0)
        {
            dummyBoard.nextTurn(false, false);
            if(endsTurn) *endsTurn = true;
            if(endsGame) *endsGame = dummyBoard.isLeaderboardFull();
            return true;
        }
        else if(*actions == 1)
        {
            actions++;
            Resident resident = (Resident)(*actions);
            if(!unmovedWarrior(resident) && !farm(resident) && !tower(resident)) return false;
            coord xF = decodeCoord(actions + 1);
            coord yF = decodeCoord(actions + 3);
            coord xT = decodeCoord(actions + 5);
            coord yT = decodeCoord(actions + 7);
            Hexagon* hex = dummyBoard.getHexagon(xF, yF);
            if(hex == nullptr) return false;
            if(!hex->place(&dummyBoard, resident, dummyBoard.getHexagon(xT, yT), false)) return false;
            actions += 9;
        }
        else if(*actions == 2)
        {
            actions++;
            coord xF = decodeCoord(actions);
            coord yF = decodeCoord(actions + 2);
            coord xT = decodeCoord(actions + 4);
            coord yT = decodeCoord(actions + 6);
            Hexagon* hex = dummyBoard.getHexagon(xF, yF);
            if(hex == nullptr) return false;
            if(!hex->move(&dummyBoard, dummyBoard.getHexagon(xT, yT), false)) return false;
            actions += 8;
        }
        else return false;
    }
    if(endsTurn) *endsTurn = false;
    if(endsGame) *endsGame = dummyBoard.isLeaderboardFull();
    return true;
}

bool executeActions(Board* board, char* actions, uint8 actionsNumber)
{
    //std::cout << "executeActions called\nNumber of actions: " << (int)actionsNumber << '\n';
    for(int i = 0; i < actionsNumber; i++)
    {
        if(*actions == 0)
        {
            //std::cout << "BNP ended turn\n";
            actions++;
            board->nextTurn(false);
            break;
        }
        else if(*actions == 1)
        {
            //std::cout << "BNP placed\n";
            actions++;
            Resident resident = (Resident)(*actions);
            if(!unmovedWarrior(resident) && !farm(resident) && !tower(resident)) return false;
            coord xF = decodeCoord(actions + 1);
            coord yF = decodeCoord(actions + 3);
            coord xT = decodeCoord(actions + 5);
            coord yT = decodeCoord(actions + 7);
            Hexagon* hex = board->getHexagon(xF, yF);
            if(hex == nullptr) return false;
            if(!hex->place(board, resident, board->getHexagon(xT, yT), false)) return false;
            actions += 9;
        }
        else if(*actions == 2)
        {
            //std::cout << "BNP moved\n";
            actions++;
            coord xF = decodeCoord(actions);
            coord yF = decodeCoord(actions + 2);
            coord xT = decodeCoord(actions + 4);
            coord yT = decodeCoord(actions + 6);
            Hexagon* hex = board->getHexagon(xF, yF);
            if(hex == nullptr) return false;
            if(!hex->move(board, board->getHexagon(xT, yT), false)) return false;
            actions += 8;
        }
        else return false;
    }
    return true;
}

BotPlayer::BotPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}

void BotPlayer::actStart()
{
    clearSocket(receiveSock);
    sendTurnChange(game->board->getCurrentPlayerId(), receiveSock);
    game->board->sendBoard(receiveSock);

    turnEndTime = glfwGetTime() + maxMoveTime;
}

void BotPlayer::act()
{
    if(glfwGetTime() >= turnEndTime)
    {
        sendConfirmation(false, false, receiveSock);
        game->board->nextTurn(false);

        char content[3]; // tag (1) + liczba akcji (1) + tag akcji (1)
        char* position = content;
        *position++ = ACTION_SOCKET_TAG; // tag
        *position++ = 1; // liczba akcji (jedna)
        *position++ = 0; // tag akcji (0 - koniec tury)
        sendData(content, sizeof(content), -1, receiveSock);

        return;
    }
    switchSocketMode(receiveSock, 1);

    char tag;
    if(recv(receiveSock, &tag, 1, 0) > 0)
    {
        if(tag == ACTION_SOCKET_TAG)
        {
            std::vector<char> data;
            if(!receiveActions(receiveSock, data))
            {
                std::cout << "Error receiving actions!\n";
                clearSocket(receiveSock);
                sendConfirmation(false, true, receiveSock);
                game->board->sendBoard(receiveSock);
                goto end;
            }

            bool endsTurn, endsGame;
            if(!canExecuteActions(game->board, id, data.data() + 2, data[1], &endsTurn, &endsGame))
            {
                sendConfirmation(false, true, receiveSock);
                game->board->sendBoard(receiveSock);
                goto end;
            }

            //gameEnded = game->board->isLeaderboardFull();
            bool awaits = !endsTurn && !endsGame;
            sendConfirmation(true, awaits, receiveSock);
            
            if(!executeActions(game->board, data.data() + 2, data[1]))
            {
                std::cout << "Actions check passed but an error occured during actions execution!\n";
                sendConfirmation(false, true, receiveSock);
                game->board->sendBoard(receiveSock);
                goto end;
            }

            if(awaits) game->board->sendBoard(receiveSock);

            sendData(data.data(), data.size(), -1, receiveSock); // Wysyłamy dane do wszystkich oprócz socketa z którego je dostaliśmy
        }
    }

end:
    switchSocketMode(receiveSock, 0);
}

NetworkPlayer::NetworkPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Network player created with max move time " << maxMoveTime << "\n";
}

void NetworkPlayer::act()
{
    switchSocketMode(receiveSock, 1);

    char tag;
    if(recv(receiveSock, &tag, 1, 0) > 0)
    {
        if(tag == ACTION_SOCKET_TAG)
        {
            std::vector<char> data;
            if(!receiveActions(receiveSock, data))
            {
                std::cout << "Error receiving network actions!\n";
                clearSocket(receiveSock);
                goto end;
            }
            
            if(!executeActions(game->board, data.data() + 2, data[1]))
            {
                std::cout << "Received unallowed actions!\n";
                clearSocket(receiveSock);
                goto end;
            }

            sendData(data.data(), data.size(), -1, receiveSock); // Wysyłamy dane do wszystkich oprócz socketa z którego je dostaliśmy
        }
        else
        {
            std::cout << "Unexpected data of tag " << (int)tag << " received, clearing socket\n";
            clearSocket(receiveSock);
        }
    }

end:
    switchSocketMode(receiveSock, 0);
}
