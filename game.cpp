#include "game.h"

#include <set>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "resource_manager.h"


// Game-related State data
SpriteRenderer* Renderer;


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

    for(int s : clientSocks)
    {
        if(receivingSocket == -1 || s == receivingSocket)
        {
            int sentBytes = 0;
            while (sentBytes < total)
            {
                int r = send(s, content + sentBytes, total - sentBytes, 0);
                if (r <= 0)
                {
                    std::cout << "Failed to send game config data\n";
                    break;
                }
                sentBytes += r;
            }
        }
    }
    
    delete[] content;
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
        for(int i = 0; i < playerMarkers.length(); i++)
        {
            if(playerMarkers[i] == 'L') players.push_back(new LocalPlayer(&countries[i],this,i+1, maxMoveTimes[i])); // rzutowanie maxMoveTimes[i] na unsigned int zmienia -1 na max unsigned int
            else if(playerMarkers[i] == 'B') players.push_back(new BotPlayer(&countries[i], maxMoveTimes[i]));
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

    // Przeliczamy macierz projekcji dla nowych wymiarów
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);

    // Aktualizujemy shader
    ResourceManager::GetShader("sprite").Use().SetMatrix4("projection", projection);
}


void Game::ProcessInput(float dt)
{
    if(scroll == -1)
    {
        Renderer -> addToResizeMultiplier(0.9,board, Width);
        scroll = 0;
    }
    if(scroll == 1)
    {
        Renderer -> addToResizeMultiplier(1.1,board,Width);
        scroll = 0;
    }
    if (pressedKey==GLFW_KEY_W)
    {
        Renderer -> addToDisplacementY(10);
    }
    if (pressedKey==GLFW_KEY_A)
    {
        Renderer ->addToDisplacementX(10);
    }
    if (pressedKey==GLFW_KEY_S)
    {
        Renderer -> addToDisplacementY(-10);
    }
    if (pressedKey==GLFW_KEY_D)
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



Player::Player(Country* country, unsigned int maxMoveTime) : country(country), maxMoveTime(maxMoveTime)
{
    country->setPlayer(this);
}

LocalPlayer::LocalPlayer(Country* country,Game *game,int myIndex, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    this->game = game;
    this->myIndex = myIndex;
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

void LocalPlayer::act()
{

    if (game->State == GameState::GAME_ACTIVE)
    {
        if(keysToResidents.contains(game->pressedKey) && game->provinceSelector!=nullptr)
        {
            std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(myIndex);
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
            game->provinceSelector = nullptr;
            game->selectedHex = nullptr;
            game->enterPressed = false;
            game->board->nextTurn();

        }
    }

}

void LocalPlayer::moveAction(Hexagon* hex,Point p)
{
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(myIndex);
    Resident res = game->board->getHexagon(p.x,p.y)->getResident();
    if (game->isHexSelected){
        std::vector<Hexagon*> nearby = game->selectedHex->possibleMovements(game->board);
        if (auto it = std::ranges::find(nearby,hex);it!=nearby.end()){
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
    std::unordered_set<Hexagon*> hexes = game->board->getHexesOfCountry(myIndex);
    if (hexes.contains(hex))
    {
        game->provinceSelector = hex;
    }
}

BotPlayer::BotPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}

void BotPlayer::act()
{
}

NetworkPlayer::NetworkPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Network player created with max move time " << maxMoveTime << "\n";
}

void NetworkPlayer::act()
{
}
