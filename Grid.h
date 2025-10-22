#ifndef GRID_H
#define GRID_H
#include "Hexagon.h"
#include "warrior.h"
#include "axial.h"
#include "player.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <cstdlib>
#include <set>
#include <algorithm>
/*
A class containg a vector of Hexagons to concatentate all vertices and indices into one
I am not sure if it is an optiml way
*/

class Grid
{

public:
    Grid() {}    
    Grid(float startX, float startY, float hexRadius);

    void AddHexagon(int q,int r);
    void AddWarrior(int q, int r);
    void AddWarriorFirst();

    void AddPlayer(glm::vec3 _color, std::string name);
    void AddHexToPlayer(int q, int r, std::string name);
    void AddWarToPlayer(int q, int r, std::string name);

    void TryToClickOnHexagon(float x, float y);
    bool CheckIfHexIsInGrid(int q, int r);
    bool CheckIfAnyWarIsInHex(int q, int r);
    void Resize(float scale);
    void Move(float dx, float dy);
    void GenerateMap(int count);

    float startX;
    float startY;
    float radius;
    Hexagon clickedHex;
    bool clicked;
    std::map<Axial,Hexagon> axialToHex;
    std::map<Axial,Warrior> axialToWar;
    std::map<std::string,Player> namesToPlayers;
    Warrior moving;
    
};
#endif