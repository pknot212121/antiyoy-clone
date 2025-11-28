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
    playerCount = playerMarkers.length();
    playerIndex = 1;
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
    board->InitializeCountriesA(playerCount, 6, 8);

    auto countries = board->getCountries();
    if(countries.size() == playerCount)
    {
        players.reserve(playerCount);
        for(int i = 0; i < playerCount; i++)
        {
            if(playerMarkers[i] == 'L') players.push_back(new LocalPlayer(&countries[i], maxMoveTimes[i])); // rzutowanie maxMoveTimes[i] na unsigned int zmienia -1 na max unsigned int
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
    //getPlayer(playerIndex)->act();
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

void Game::moveAction(Hexagon* hex,Point p)
{
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(playerIndex);
    Resident res = board->getHexagon(p.x,p.y)->getResident();
    if (isHexSelected){
        std::vector<Hexagon*> nearby = selectedHex->possibleMovements(board);
        if (auto it = std::ranges::find(nearby,hex);it!=nearby.end()){
            selectedHex->move(board,hex);
        } else std::cout << "MOVEMENT NOT POSSIBLE\n";
        Renderer -> ClearBrightenedHexes();
        isHexSelected=false;
    }
    else if ((res==Resident::Warrior1 || res==Resident::Warrior2 || res==Resident::Warrior3 || res==Resident::Warrior4) && hexes.contains(hex))
    {
        selectedHex=hex;isHexSelected=true;
        std::vector<Hexagon*> nearby = selectedHex->possibleMovements(board);
        Renderer -> setBrightenedHexes(nearby);
    }

}

void Game::spawnAction(Hexagon* hex,Point p)
{
    if (provinceSelector!=nullptr)
    {
        std::vector<Hexagon*> neigh = provinceSelector->possiblePlacements(board,Resident::Warrior1);
        if (auto it = std::ranges::find(neigh,hex); it!=neigh.end()){
            provinceSelector->place(board,keysToResidents[pressedKey],hex);
        }
        Renderer -> ClearBrightenedHexes();
    }

}

void Game::SelectAction(Hexagon *hex,Point p)
{
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(playerIndex);
    if (hexes.contains(hex))
    {
        provinceSelector = hex;
    }
}



void Game::ProcessInput(float dt)
{
   
    if (this->State == GameState::GAME_ACTIVE)
    {
        if(keysToResidents.contains(pressedKey) && provinceSelector!=nullptr)
        {
            std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(playerIndex);
            std::vector<Hexagon*> neigh = provinceSelector->possiblePlacements(board,Resident::Warrior1);
            Renderer -> setBrightenedHexes(neigh);
        }
        if (this->mousePressed)
        {
            float size = Width / board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * board->getWidth();
            Point p = Renderer -> CheckWhichHexagon(cursorPosX,cursorPosY,size/2);
            Hexagon *hex = board->getHexagon(p.x,p.y);
            if (p.x<board->getWidth() && p.x>=0 && p.y<board->getHeight() && p.y>=0)
            {

                if(keysToResidents.contains(pressedKey) && !isHexSelected){
                    this->spawnAction(hex,p);
                }
                else
                {
                    this->moveAction(hex,p);
                    this->SelectAction(hex,p);
                }
                this -> mousePressed = false;
            }


        }
        if (!keysToResidents.contains(pressedKey) && isHexSelected==false && provinceSelector!=nullptr)
        {
            Renderer->ClearBrightenedHexes();
        }
        if(this->scroll == -1)
        {
            Renderer -> addToResizeMultiplier(0.9,board, this->Width);
            scroll = 0;
        }
        if(this->scroll == 1)
        {
            Renderer -> addToResizeMultiplier(1.1,board,this->Width);
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
        if(pressedKey==GLFW_KEY_ENTER)
        {
            enterPressed = true;

        }
        if(pressedKey!=GLFW_KEY_ENTER && enterPressed)
        {
            provinceSelector = nullptr;
            selectedHex = nullptr;
            playerIndex = (playerIndex)%playerCount+1;
            enterPressed = false;
        }
    }


}

void Game::Render()
{
    Renderer -> DrawBoard(board, this->Width, this->Height,playerIndex);
    std::unordered_map<Hexagon*, int>& m= board->getCountry(playerIndex)->getCastles();
    int sum=0;
    if (provinceSelector!=nullptr)
    {
        for (auto a : m)
        {
            if (provinceSelector->province(board)[0]==a.first)
            {
                sum+=a.second;
                break;
            }
        }
    }
    Text->RenderText("Money:"+std::to_string(sum) ,10.0f, 10.0f, 1.0f);
}



Player::Player(Country* country, unsigned int maxMoveTime) : country(country), maxMoveTime(maxMoveTime)
{
    country->setPlayer(this);
}

LocalPlayer::LocalPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Local player created with max move time " << maxMoveTime << "\n";
}

void LocalPlayer::act()
{
}

BotPlayer::BotPlayer(Country* country, unsigned int maxMoveTime) : Player(country, maxMoveTime)
{
    std::cout << "Bot player created with max move time " << maxMoveTime << "\n";
}

void BotPlayer::act()
{
}