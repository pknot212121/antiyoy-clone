#ifndef GRID_H
#define GRID_H
#include <vector>
#include "Hexagon.h"
#include "warrior.h"
#include "axial.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
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

    float startX;
    float startY;
    float radius;
    Hexagon clickedHex;
    std::vector<Hexagon> hexagons;
    bool clicked;
    std::map<Axial,Hexagon> axialToHex;
    std::vector<Warrior> warriors;
    std::map<Axial,Warrior> axialToWar;
    Warrior moving;
    
};
#endif