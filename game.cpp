#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "Hexagon.h"
#include "Grid.h"


// Game-related State data
SpriteRenderer  *Renderer;


Game::Game(unsigned int width, unsigned int height) 
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{ 

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


    this -> grid = Grid(300.0f,300.0f,100.0f);
    grid.AddHexagon(1,0);
    grid.AddHexagon(-1,0);
    grid.AddHexagon(0,1);
    grid.AddHexagon(0,2);
    grid.AddHexagon(0,-1);
    grid.AddHexagon(-1,-1);
    grid.AddWarrior(0,1);
}

void Game::Update(float dt)
{
    
}

void Game::ProcessInput(float dt)
{
   
    if (this->State == GAME_ACTIVE)
    {
        // move playerboard
        if (this->mousePressed)
        {
            std::cout << "POSITION_X: " << cursorPosX << " POSITION_Y: " << cursorPosY << std::endl;
            grid.CheckWhichHexagon((float)cursorPosX,(float)cursorPosY);
            this -> mousePressed = false;
        }
    }
}

void Game::Render()
{
    // Renderer->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(-100.0f, 0.0f), glm::vec2(300.0f, 300.0f * sqrt(3)/2), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    // Renderer->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(300.0f, 200.0f), glm::vec2(300.0f, 300.0f * sqrt(3)/2), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    // Renderer -> DrawHexagon(100.0f,100.0f,100.0f,glm::vec3(0.0f,1.0f,0.0f));
    // Renderer -> DrawHexagon(Hexagon(200.0f,200.0f,100.0f,glm::vec3(0.0f,1.0f,0.0f)));
    Renderer -> DrawGrid(grid);

    // Renderer -> DrawSprite(ResourceManager::GetTexture("level1warrior"), glm::vec2(500.0f, 0.0f), glm::vec2(100.0f, 100.0f), 0.0f, glm::vec3(1.0f, 1.0f, 1.0f));

}