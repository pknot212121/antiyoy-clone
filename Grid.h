#ifndef GRID_H
#define GRID_H
#include <vector>
#include "Hexagon.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
    
    void CheckWhichHexagon(int x, int y, int SCR_WIDTH, int SCR_HEIGHT);
    bool CheckIfHexIsInGrid(int q, int r);

    float startX;
    float startY;
    float radius;
    Hexagon clickedHex;
    std::vector<Hexagon> hexagons;
    bool clicked;
};
#endif