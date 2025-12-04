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

inline std::map<int,Resident> keysToResidents {
{GLFW_KEY_1,Resident::Warrior1},
{GLFW_KEY_2,Resident::Warrior2},
{GLFW_KEY_3,Resident::Warrior3},
{GLFW_KEY_4,Resident::Warrior4},
{GLFW_KEY_5,Resident::Farm},
{GLFW_KEY_6,Resident::Tower},
{GLFW_KEY_7,Resident::StrongTower}
};

inline Resident keysToR[] = {
    Resident::Warrior1,
    Resident::Warrior2,
    Resident::Warrior3,
    Resident::Warrior4,
    Resident::Farm,
    Resident::Tower,
    Resident::StrongTower
};

class GameConfigData
{
public:
    coord x, y;
    unsigned int seed;
    int minProvinceSize, maxProvinceSize;
    std::string playerMarkers;
    std::vector<int> maxMoveTimes;

    GameConfigData() = default;
    GameConfigData(coord x, coord y, unsigned int seed, int minProvinceSize, int maxProvinceSize, std::string playerMarkers, std::vector<int> maxMoveTimes);

    inline int estimateSize() { return sizeof(x) + sizeof(y) + sizeof(seed) + sizeof(minProvinceSize) + sizeof(maxProvinceSize) + 1 + playerMarkers.length() + 1 + maxMoveTimes.size() * sizeof(int); };
    void sendGameConfigData(int receivingSocket = -1);
    bool receiveFromSocket(int deliveringSocket, bool tag = false);
};

// Game holds all game-related state and functionality.
// Combines all game-related data into a single class for
// easy access to each of the components and manageability.
class Game
{
public:
    // game state
    GameState               State;	
    int                     pressedKey = -1;
    bool                    mousePressed;
    float                   cursorPosX;
    float                   cursorPosY;
    int                     scroll = 0;
    unsigned int            Width, Height;
    bool                    enterPressed = false;
    bool                    isHexSelected = false;
    bool                    isFirstProvinceSet = false;
    Hexagon                 *selectedHex = nullptr;
    Hexagon                 *provinceSelector = nullptr;
    std::vector<Player*>    players;
    SpriteRenderer* Renderer;
    inline Player* getPlayer(uint8 id) noexcept { return (id == 0) ? nullptr : players[id-1]; }

    Board *board;
    TextRenderer  *Text;
    // constructor/destructor
    Game(unsigned int width, unsigned int height);
    ~Game();
    // initialize game state (load all shaders/textures/levels)
    void Init(GameConfigData& gcd);
    // game loop
    void ProcessInput(float dt);
    int GetSelectedCastleReserves();
    int GetSelectedCastleIncome();
    void RefreshSprites();
    void RefreshOutline();
    void Update(float dt);
    void Resize(int width, int height);
    void Render();
    std::map<int,bool> clickedMovingKeys{
        {GLFW_KEY_W,false},
        {GLFW_KEY_A,false},
        {GLFW_KEY_S,false},
        {GLFW_KEY_D,false}
    };
};


void executeActions(Board* board, char* actions, uint8 actionsNumber);


class Player
{
protected:
    Country* country;
    uint8 id;
    Game* game;
    unsigned int maxMoveTime;

public:
    Player(Country* country, uint8 id, Game* game, unsigned int maxMoveTime = 60);

    //virtual void actStart();
    virtual void act() = 0; // udawaj że to funkcja abstrakcyjna
};

class LocalPlayer : public Player
{
public:
    LocalPlayer(Country* country, uint8 id, Game* game, unsigned int maxMoveTime = 60);
    virtual void act();

    void moveAction(Hexagon* hex, Point p);
    void spawnAction(Hexagon* hex, Point p);
    void SelectAction(Hexagon* hex, Point p);
};

class BotPlayer : public Player
{
    int receiveSock;
public:
    BotPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime = 10);
    //virtual void actStart();
    virtual void act();
};

class NetworkPlayer : public Player // Easter egg
{
    int receiveSock;
public:
    NetworkPlayer(Country* country, uint8 id, Game* game, int receiveSock, unsigned int maxMoveTime = 60);
    virtual void act();
};

#endif