#include "game.h"
#include "resource_manager.h"



// Game-related State data
SpriteRenderer  *Renderer;


Game::Game(unsigned int width, unsigned int height)
    : State(GameState::GAME_ACTIVE), Keys(), Width(width), Height(height), board() {
}

Game::~Game()
{
    delete Renderer;
}

void Game::Init()
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
    ResourceManager::LoadTexture("textures/hexagon.png", true, "hexagon");
    ResourceManager::LoadTexture("textures/level1warrior.png",true,"level1warrior");

    coord x = 10;
    coord y = 10;
    this -> board = new Board(x, y);
    int total = x * y;
    board->InitializeRandomA(0, total * 0.5, total * 0.9); // zapełniamy 50%-90% mapy
    //board->InitializeNeighbour(2, false); // inicjalizacja sąsiadowa (odkomentuj i zakomentuj tą wyżej by zobaczyć)

    board->InitializeCountriesA(0, 3, 6, 8);

    // this -> grid = Grid(300.0f,300.0f,100.0f);
    // // grid.AddHexagon(1,0);
    // // grid.AddHexagon(-1,0);
    // // grid.AddHexagon(0,1);
    // // grid.AddHexagon(0,2);
    // // grid.AddHexagon(0,-1);
    // // grid.AddHexagon(-1,-1);
    // grid.GenerateMap(100);
    //
    // grid.AddPlayer(glm::vec3(0.0f,1.0f,0.0f),"tk2");
    // grid.AddPlayer(glm::vec3(1.0f,0.0f,0.0f),"tk3");
    //
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk2");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk3");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk2");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk3");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk2");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk3");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk2");
    // grid.AddWarToPlayer(grid.GetRandomHex(),"tk3");
    // grid.currentPlayer = grid.names[0];
}

void Game::Update(float dt)
{
    
}

void Game::ProcessInput(float dt)
{
   
    if (this->State == GameState::GAME_ACTIVE)
    {
        // move playerboard
        // if (this->mousePressed)
        // {
        //     std::cout << "POSITION_X: " << cursorPosX << " POSITION_Y: " << cursorPosY << std::endl;
        //     grid.TryToClickOnHexagon((float)cursorPosX,(float)cursorPosY);
        //     this -> mousePressed = false;
        // }
        // if(this->scroll == -1)
        // {
        //     grid.Resize(0.9);
        //     scroll = 0;
        // }
        // if(this->scroll == 1)
        // {
        //     grid.Resize(1.1);
        //     scroll = 0;
        // }
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
    }
        // if(this->Keys[GLFW_KEY_ENTER])
        // {
        //     enterPressed = true;
        //
        // }
        // if(!this->Keys[GLFW_KEY_ENTER] && enterPressed)
        // {
        //     playerIndex++;
        //     grid.currentPlayer = grid.names[playerIndex%(grid.names.size())];
        //     std::cout << "CURRENT PLAYER: " << grid.currentPlayer << std::endl;
        //     enterPressed = false;
        // }

}

void Game::Render()
{
    // Renderer->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(-100.0f, 0.0f), glm::vec2(300.0f, 300.0f * sqrt(3)/2), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    // Renderer->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(300.0f, 200.0f), glm::vec2(300.0f, 300.0f * sqrt(3)/2), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    // Renderer -> DrawHexagon(100.0f,100.0f,100.0f,glm::vec3(0.0f,1.0f,0.0f));
    // Renderer -> DrawHexagon(Hexagon(200.0f,200.0f,100.0f,glm::vec3(0.0f,1.0f,0.0f)));
    Renderer -> DrawBoard(board, this->Width, this->Height);

    // Renderer -> DrawSprite(ResourceManager::GetTexture("level1warrior"), glm::vec2(500.0f, 0.0f), glm::vec2(100.0f, 100.0f), 0.0f, glm::vec3(1.0f, 1.0f, 1.0f));

}