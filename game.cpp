#include "game.h"

#include <set>

#include "resource_manager.h"





// Game-related State data
SpriteRenderer  *Renderer;


Game::Game(unsigned int width, unsigned int height)
    : State(GameState::GAME_ACTIVE), Keys(), Width(width), Height(height), board(), selectedHex(Point(0,0)) {
}

Game::~Game()
{
    delete Renderer;
}

void Game::Init()
{
    playerCount = 3;
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
    ResourceManager::LoadTexture("textures/soilder1_256.png",true,"s1");
    ResourceManager::LoadTexture("textures/hexagon.png", true, "hexagon");
    ResourceManager::LoadTexture("textures/level1warrior.png",true,"lw");


    coord x = 10;
    coord y = 10;
    board = new Board(x, y, this);
    int total = x * y;
    board->InitializeRandomA(0, total * 0.5, total * 0.9); // zapełniamy 50%-90% mapy
    //board->InitializeNeighbour(2, false); // inicjalizacja sąsiadowa (odkomentuj i zakomentuj tą wyżej by zobaczyć)
    board->InitializeCountriesA(0, playerCount, 6, 8);

    if(board->getCountries().size() == playerCount)
    {
        players.reserve(playerCount);
        for(Country& country : board->getCountries())
        {
            players.emplace_back(&country);
        }
    }
    else std::cout << "Countries initialization error\n";

}

void Game::Update(float dt)
{
    
}

void Game::moveAction()
{
    float size = Width / board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * board->getWidth();
    Point p = Renderer -> CheckWhichHexagon(cursorPosX,cursorPosY,size/2);
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(playerIndex);
    Hexagon *hex = board->getHexagon(p.x,p.y);
    std::set<Hexagon*> orderedHexes(hexes.begin(),hexes.end());
    std::vector<Hexagon*> neigh = (*hexes.begin())->possiblePlacements(board,Resident::Warrior1);
    if (board->getHexagon(p.x,p.y)->getResident()==Resident::Warrior1){board->getHexagon(p.x,p.y)->mark();}
    else{
        for (auto& element : orderedHexes){
            auto it = std::ranges::find(neigh,hex);
            if (element->marked() && element->getResident()==Resident::Warrior1 && it!=neigh.end()){
                element->move(board,hex);
                element->unmark();
            }
        }
    }
}

void Game::spawnAction()
{
    float size = Width / board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * board->getWidth();
    Point p = Renderer -> CheckWhichHexagon(cursorPosX,cursorPosY,size/2);
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(playerIndex);
    Hexagon *hex = board->getHexagon(p.x,p.y);
    std::set<Hexagon*> orderedHexes(hexes.begin(),hexes.end());
    std::vector<Hexagon*> neigh = (*hexes.begin())->possiblePlacements(board,Resident::Warrior1);
    if (auto it = std::ranges::find(neigh,hex); it!=neigh.end()){
        board->getHexagon(p.x,p.y)->setResident(Resident::Warrior1);
        board->getHexagon(p.x,p.y)->setOwnerId(playerIndex);
    }
}



void Game::ProcessInput(float dt)
{
   
    if (this->State == GameState::GAME_ACTIVE)
    {
        if (this->mousePressed)
        {
            this->moveAction();
            if(this->onePressed){
                this->spawnAction();
                onePressed=false;
            }
            this -> mousePressed = false;
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
        if (this->Keys[GLFW_KEY_W])
        {
            Renderer -> addToDisplacementY(10);
        }
        if (this->Keys[GLFW_KEY_A])
        {
            Renderer ->addToDisplacementX(10);
        }
        if (this->Keys[GLFW_KEY_S])
        {
            Renderer -> addToDisplacementY(-10);
        }
        if (this->Keys[GLFW_KEY_D])
        {
            Renderer -> addToDisplacementX(-10);
        }
        if (this->Keys[GLFW_KEY_1])
        {
            onePressed=true;
        }
        if (this->Keys[GLFW_KEY_2])
        {

        }
        if (this->Keys[GLFW_KEY_3])
        {

        }
        if (this->Keys[GLFW_KEY_4])
        {

        }
        if(this->Keys[GLFW_KEY_ENTER])
        {
            enterPressed = true;

        }
        if(!this->Keys[GLFW_KEY_ENTER] && enterPressed)
        {
            playerIndex = (playerIndex)%playerCount+1;
            enterPressed = false;
        }
    }


}

void Game::Render()
{
    Renderer -> DrawBoard(board, this->Width, this->Height);
}

