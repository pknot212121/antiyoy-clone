#ifndef GAME_H
#define GAME_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "sprite_renderer.h"
#include <iostream>
#include <ranges>
#include "board.h"


// Represents the current state of the game
enum class GameState
{
    GAME_ACTIVE,
    GAME_MENU,
    GAME_WIN
};

// Game holds all game-related state and functionality.
// Combines all game-related data into a single class for
// easy access to each of the components and manageability.
class Game
{
public:
    // game state
    GameState               State;	
    bool                    Keys[1024];
    bool                    mousePressed;
    float                   cursorPosX;
    float                   cursorPosY;
    int                     scroll = 0;
    unsigned int            Width, Height;
    coord                   playerCount;
    coord                   playerIndex;
    bool                    enterPressed = false;
    bool                    onePressed = false;
    bool                    isHexSelected = false;
    Hexagon                 *selectedHex = nullptr;
    Hexagon                 *provinceSelector = nullptr;
    std::vector<Player*>     players;

    Board *board;
    // constructor/destructor
    Game(unsigned int width, unsigned int height);
    ~Game();
    // initialize game state (load all shaders/textures/levels)
    void Init(coord x, coord y, int seed, std::string playerMarkers, std::vector<int> maxMoveTimes);
    // game loop
    void ProcessInput(float dt);
    void Update(float dt);
    void Resize(int width, int height);
    void Render();
    void moveAction(Hexagon* hex, Point p);
    void spawnAction(Hexagon* hex, Point p);
    void SelectAction(Point p);
};

#endif