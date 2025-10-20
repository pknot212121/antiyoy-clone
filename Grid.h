#ifndef GRID_H
#define GRID_H
#include <vector>
#include "Hexagon.h"
#include "warrior.h"
#include "axial.h"
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
    
    void CheckWhichHexagon(float x, float y);
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
    Warrior moving;
    
};
#endif