#include "game.h"

#include <set>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "resource_manager.h"


// Game-related State data



GameConfigData::GameConfigData(coord x, coord y, unsigned int seed, int minProvinceSize, int maxProvinceSize, std::string playerMarkers, std::vector<int> maxMoveTimes)
: x(x), y(y), seed(seed), minProvinceSize(minProvinceSize), maxProvinceSize(maxProvinceSize), playerMarkers(playerMarkers), maxMoveTimes(maxMoveTimes)
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

void Game::Init(GameConfigData& gcd)
{
    // load shaders
    ResourceManager::LoadShader("shaders/instance.vs","shaders/instance.fs",nullptr,"sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width), 
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    // set render-specific controls

    // load textures
    ResourceManager::LoadTexture("textures/soilder1_256.png",true,"soilder1");
    ResourceManager::LoadTexture("textures/soldier2_256.png",true,"soilder2");
    ResourceManager::LoadTexture("textures/soldier3_256.png",true,"soilder3");
    ResourceManager::LoadTexture("textures/hexagon.png", true, "hexagon");
    ResourceManager::LoadTexture("textures/placeholder.png",true,"placeholder");
    ResourceManager::LoadTexture("textures/exclamation.png",true,"exclamation");
    ResourceManager::LoadTexture("textures/castle_256.png",true,"castle");
    ResourceManager::LoadTexture("textures/pineTree_256.png",true,"pine");
    ResourceManager::LoadTexture("textures/palmTree_256.png",true,"palm");
    ResourceManager::LoadTexture("textures/tower_256.png",true,"tower");
    ResourceManager::LoadTexture("textures/gravestone_256.png",true,"gravestone");
    ResourceManager::LoadTexture("textures/shield_placeholder.png",true,"shield");
    ResourceManager::LoadTexture("textures/b.png",true,"border_placeholder");
    ResourceManager::LoadTexture("textures/farm1_256.png",true,"farm1");
    ResourceManager::LoadTexture("textures/strongTower_256.png",true,"strongTower");

    Text = new TextRenderer(this->Width, this->Height);
    Text->Load(24);
    if(gcd.seed == 0) gcd.seed = std::random_device{}();
    gen = std::mt19937(gcd.seed);

    std::string markers = gcd.playerMarkers;
    uint8 playersNumber = markers.length();

    bool isHost = clientSocks.size() > 0;
    board = new Board(gcd.x, gcd.y, this);
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"),gcd.x,gcd.y);
    int total = gcd.x * gcd.y;
    board->InitializeRandomWithAnts(5,total * 0.3, total * 0.5);
    board->InitializeCountries(playersNumber, gcd.minProvinceSize, gcd.maxProvinceSize);
    board->spawnTrees(0.2);
    Renderer->width = Width;
    Renderer->height = Height;
    Renderer->size = Renderer->getSize(board);


    auto countries = board->getCountries();
    //std::cout << countries.size() << " " << (int)playersNumber << " " << gcd.maxMoveTimes.size() << '\n';
    if(countries.size() == playersNumber)
    {
        players.reserve(playersNumber);
        uint8 networkSockIndex = 0;
        for(uint8 i = 0; i < playersNumber; i++) // Jeśli mamy choć jednego bota to pierwszy socket należy do botów
        {
            if(markers[i] == 'B')
            {
                gcd.sendGameConfigData(clientSocks[0]);
                networkSockIndex = 1;
                break;
            }
        }

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
    }
    else
    {
        std::cout << "Countries initialization error\n";
        getchar();
        std::exit(1);
    }
    std::cout << "Finished init\n";

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
    if (clickedMovingKeys[GLFW_KEY_W])
    {
        Renderer -> addToDisplacementY(board,10);
    }
    if (clickedMovingKeys[GLFW_KEY_A])
    {
        Renderer ->addToDisplacementX(board,10);
    }
    if (clickedMovingKeys[GLFW_KEY_S])
    {
        Renderer -> addToDisplacementY(board,-10);
    }
    if (clickedMovingKeys[GLFW_KEY_D])
    {
        Renderer -> addToDisplacementX(board,-10);
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

LocalPlayer::LocalPlayer(Country* country, uint8 id, Game* game, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime)
{
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

void LocalPlayer::act()
{

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
        if(game->pressedKey!=GLFW_KEY_ENTER && game->enterPressed)
        {
            game->enterPressed = false;
            game->isFirstProvinceSet = false;
            game->provinceSelector=nullptr;
            game->Renderer->shieldHexes.clear();
            game->board->nextTurn(true);

        }
    }

void LocalPlayer::moveAction(Hexagon* hex,glm::ivec2 p)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(id);
    Resident res = game->board->getHexagon(p.x,p.y)->getResident();
    if (game->isHexSelected){
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        if (auto it = std::ranges::find(nearby,hex);it!=nearby.end()){
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
        if (auto it = std::ranges::find(neigh,hex); it!=neigh.end()){
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

BotPlayer::BotPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}

/*void BotPlayer::actStart()
{
    sendTurnChange(id);
}*/

void BotPlayer::act()
{
    switchSocketMode(receiveSock, 1);

    char tag;
    if(recv(receiveSock, &tag, 1, 0) > 0)
    {
        if(tag == ACTION_SOCKET_TAG)
        {
            switchSocketMode(receiveSock, 0);
            uint8 actionsNumber;
            if(recv(receiveSock, reinterpret_cast<char*>(&actionsNumber), 1, 0) > 0)
            {
                std::vector<char> data = {tag, static_cast<char>(actionsNumber)};
                for(int i = 0; i < actionsNumber; i++)
                {
                    char action;
                    if(recv(receiveSock, &action, 1, 0) > 0)
                    {
                        data.push_back(action);
                        if(action == 0)
                        {
                            break;
                        }
                        else if(action == 1)
                        {
                            char content[9];
                            int total = 0;
                            while (total < sizeof(content))
                            {
                                int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                                if (r <= 0) goto error;
                                total += r;
                            }
                            data.insert(data.end(), content, content + sizeof(content));
                        }
                        else if(action == 2)
                        {
                            char content[8];
                            int total = 0;
                            while (total < sizeof(content))
                            {
                                int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                                if (r <= 0) goto error;
                                total += r;
                            }
                            data.insert(data.end(), content, content + sizeof(content));
                        }
                        else goto invalidContent;
                    }
                    else goto error;
                }

                executeActions(game->board, data.data() + 2, actionsNumber);

                sendData(data.data(), data.size(), -1, receiveSock); // Wysyłamy dane do wszystkich oprócz socketa z którego je dostaliśmy
            }
            else goto error;
        }
    }
    else switchSocketMode(receiveSock, defaultSocketMode);

    return;
error:
    switchSocketMode(receiveSock, defaultSocketMode);
    // Coś wymyślę
    std::cout << "Read error\n";
    return;

invalidContent:
    switchSocketMode(receiveSock, defaultSocketMode);
    // Tu też
    std::cout << "Invalid content\n";
    return;
}

NetworkPlayer::NetworkPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime) : Player(country, id, game, maxMoveTime), receiveSock(receiveSock)
{
    std::cout << "Network player created with max move time " << maxMoveTime << "\n";
}

void executeActions(Board* board, char* actions, uint8 actionsNumber)
{
    for(int i = 0; i < actionsNumber; i++)
    {
        if(*actions == 0)
        {
            //std::cout << "NP ended turn\n";
            actions++;
            board->nextTurn(false);
            break;
        }
        else if(*actions == 1)
        {
            //std::cout << "NP placed\n";
            actions++;
            Resident resident = (Resident)(*actions);
            coord xF = decodeCoord(actions + 1);
            coord yF = decodeCoord(actions + 3);
            coord xT = decodeCoord(actions + 5);
            coord yT = decodeCoord(actions + 7);
            Hexagon* hex = board->getHexagon(xF, yF);
            hex->place(board, resident, board->getHexagon(xT, yT), false);
            actions += 9;
        }
        else if(*actions == 2)
        {
            //std::cout << "NP moved\n";
            actions++;
            coord xF = decodeCoord(actions);
            coord yF = decodeCoord(actions + 2);
            coord xT = decodeCoord(actions + 4);
            coord yT = decodeCoord(actions + 6);
            Hexagon* hex = board->getHexagon(xF, yF);
            hex->move(board, board->getHexagon(xT, yT), false);
            actions += 8;
        }
        else return;
    }
}

void NetworkPlayer::act()
{
    switchSocketMode(receiveSock, 1);

    char tag;
    if(recv(receiveSock, &tag, 1, 0) > 0)
    {
        if(tag == ACTION_SOCKET_TAG)
        {
            switchSocketMode(receiveSock, 0);
            uint8 actionsNumber;
            if(recv(receiveSock, reinterpret_cast<char*>(&actionsNumber), 1, 0) > 0)
            {
                std::vector<char> data = {tag, static_cast<char>(actionsNumber)};
                for(int i = 0; i < actionsNumber; i++)
                {
                    char action;
                    if(recv(receiveSock, &action, 1, 0) > 0)
                    {
                        data.push_back(action);
                        if(action == 0)
                        {
                            break;
                        }
                        else if(action == 1)
                        {
                            char content[9];
                            int total = 0;
                            while (total < sizeof(content))
                            {
                                int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                                if (r <= 0) goto error;
                                total += r;
                            }
                            data.insert(data.end(), content, content + sizeof(content));
                        }
                        else if(action == 2)
                        {
                            char content[8];
                            int total = 0;
                            while (total < sizeof(content))
                            {
                                int r = recv(receiveSock, content + total, sizeof(content) - total, 0);
                                if (r <= 0) goto error;
                                total += r;
                            }
                            data.insert(data.end(), content, content + sizeof(content));
                        }
                        else goto invalidContent;
                    }
                    else goto error;
                }

                executeActions(game->board, data.data() + 2, actionsNumber);

                sendData(data.data(), data.size(), -1, receiveSock); // Wysyłamy dane do wszystkich oprócz socketa z którego je dostaliśmy
            }
            else goto error;
        }
    }
    else switchSocketMode(receiveSock, defaultSocketMode);

    return;
error:
    switchSocketMode(receiveSock, defaultSocketMode);
    // Coś wymyślę
    std::cout << "Read error\n";
    return;

invalidContent:
    switchSocketMode(receiveSock, defaultSocketMode);
    // Tu też
    std::cout << "Invalid content\n";
    return;
}
