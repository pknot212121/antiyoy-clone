#ifndef GAME_H
#define GAME_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "sprite_renderer.h"
#include <iostream>
#include <ranges>
#include "board.h"
#include "text_renderer.h"


// Represents the current state of the game
enum class GameState
{
    GAME_ACTIVE,
    GAME_MENU,
    GAME_WIN
};

class GameConfigData
{
    coord x, y;
    int seed;
    std::string playerMarkers;

    GameConfigData(coord x, coord y, int seed, std::string playerMarkers);
    GameConfigData(char* data);

    inline int estimateSize() { return sizeof(x) + sizeof(y) + sizeof(seed) + 1 + playerMarkers.length(); };
    void sendGameConfigData(int receivingSocket = -1);
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
    uint8                   playerCount;
    uint8                   playerIndex;
    bool                    enterPressed = false;
    bool                    onePressed = false;
    bool                    isHexSelected = false;
    Hexagon                 *selectedHex = nullptr;
    Hexagon                 *provinceSelector = nullptr;
    std::vector<Player*>     players;

    inline Player* getPlayer(uint8 id) noexcept { return (id == 0) ? nullptr : players[id-1]; }

    Board *board;
    TextRenderer  *Text;
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
    void SelectAction(Hexagon* hex, Point p);
};


class Player
{
    Country* country;
    unsigned int maxMoveTime;

public:
    Player(Country* country, unsigned int maxMoveTime = 60);

    virtual void act() = 0; // udawaj że to funkcja abstrakcyjna
};

class LocalPlayer : public Player
{
public:
    LocalPlayer(Country* country, unsigned int maxMoveTime = 60);
    virtual void act();
};

class BotPlayer : public Player
{
public:
    BotPlayer(Country* country, unsigned int maxMoveTime = 10);
    virtual void act();
};

class NetworkPlayer : Player // Easter egg
{

};

#endif