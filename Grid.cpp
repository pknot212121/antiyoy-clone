#include "Grid.h"
#include<cmath>
#include<iostream>



Grid::Grid(float _startX, float _startY, float hexRadius)
{
    Hexagon hex = Hexagon(_startX,_startY,hexRadius,glm::vec3(0.5f,0.5f,0.5f));
    hexagons.push_back(hex);
    startX = _startX;
    startY = _startY;
    radius = hexRadius;
}

void Grid::AddHexagon(int q,int r)
{ 
    Hexagon hex = Hexagon(startX+radius*3/2*q-radius*3/2*r,startY-(radius*sqrt(3)/2*q+radius*sqrt(3)/2*r),radius,glm::vec3(0.5f,0.5f,0.5f));
    hex.q = q;
    hex.r = r;
    hexagons.push_back(hex);
}

void Grid::CheckWhichHexagon(int x, int y, int SCR_WIDTH, int SCR_HEIGHT)
{
    float normalisedX = ((float)x - ((float)SCR_WIDTH)/2)/(float)SCR_WIDTH * 2;
    float normalisedY = ((float)y - ((float)SCR_HEIGHT)/2) * (-1) / (float)SCR_HEIGHT * 2;
    float a = 0.3f;
    // std::cout << "NORMALIZEDX: " << normalisedX << " NORMALIZEDY: " << normalisedY << std::endl;

    float r = (sqrt(3)*normalisedY-normalisedX) / a / 3;
    float q = (sqrt(3)*normalisedY+normalisedX) / a / 3;

    // std::cout << "R: " << r << " Q: " << q << std::endl;
    int r_rounded = (int)r, q_rounded = (int)q;

    // std::cout << "DIFFERENCE: " << std::abs(r-r_rounded) << std::endl;
    if(std::abs(r-r_rounded)>=0.5){ r_rounded+= (r/std::abs(r));}
    if(std::abs(q-q_rounded)>=0.5){ q_rounded+= (q/std::abs(q));}

    
    std::cout << "roundedR: " << r_rounded << " roundedQ: " << q_rounded << std::endl;
    if(!clicked)
    {
        // if(CheckIfHexIsInGrid(q_rounded,r_rounded)){SaveHexagonFromCoords(q_rounded,r_rounded); }
    }
    else {clicked=false;}
    
}

bool Grid::CheckIfHexIsInGrid(int q, int r)
{
    for(Hexagon hex : hexagons){
        if(hex.q == q && hex.r == r){
            std::cout << "HEX on Q=" << q << " and R=" << r << " FOUND" << std::endl;
            return true;
        }
    }
    std::cout << "NOT FOUND" << std::endl;
    return false;
}
