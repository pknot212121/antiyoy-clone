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

    axialToHex[Axial(0,0)] = hex;
}

void Grid::AddHexagon(int q,int r)
{ 
    Hexagon hex = Hexagon(startX+radius*3/2*q-radius*3/2*r,startY-(radius*sqrt(3)/2*q+radius*sqrt(3)/2*r),radius,glm::vec3(0.5f,0.5f,0.5f));
    hex.q = q;
    hex.r = r;
    hexagons.push_back(hex);
    axialToHex[Axial(q,r)] = hex;
}

void Grid::CheckWhichHexagon(float x, float y)
{
    float dx = x - startX;
    float dy = y - startY;
    // float a = 0.3f;
    // std::cout << "NORMALIZEDX: " << normalisedX << " NORMALIZEDY: " << normalisedY << std::endl;


    float q = (sqrt(3)*dy-dx) / radius / 3 * (-1);
    float r = (sqrt(3)*dy+dx) / radius / 3 * (-1);

    // std::cout << "R: " << r << " Q: " << q << std::endl;
    int r_rounded = (int)r, q_rounded = (int)q;

    // std::cout << "DIFFERENCE: " << std::abs(r-r_rounded) << std::endl;
    if(std::abs(r-r_rounded)>=0.5){ r_rounded+= (r/std::abs(r));}
    if(std::abs(q-q_rounded)>=0.5){ q_rounded+= (q/std::abs(q));}

    
    std::cout << "roundedQ: " << q_rounded << " roundedR: " << r_rounded << std::endl;
    if(!clicked)
    {
        if(CheckIfHexIsInGrid(q_rounded,r_rounded) && CheckIfAnyWarIsInHex(q_rounded,r_rounded)){
            clicked = true;
            moving = axialToWar[Axial(q_rounded,r_rounded)];
            
        }
        std::cout << "--------------------------------------------" << std::endl;
            for (auto i = axialToWar.begin(); i != axialToWar.end(); i++)
                std::cout << i->first.q << i->first.r << " -- " << i->second.hex.q << i->second.hex.r << std::endl;
        std::cout << "--------------------------------------------" << std::endl;
    }
    else {
        
        if(CheckIfHexIsInGrid(q_rounded,r_rounded))
        {
            Hexagon &hex = axialToHex[Axial(q_rounded,r_rounded)];
            axialToWar.erase(Axial(moving.hex.q,moving.hex.r));
            moving.hex = hex;
            axialToWar[Axial(q_rounded,r_rounded)] = moving;
            std::cout << "MOVING WARRIOR TO: " << hex.q << " " << hex.r;
            std::cout << "--------------------------------------------" << std::endl;
            for (auto i = axialToWar.begin(); i != axialToWar.end(); i++)
                std::cout << i->first.q << i->first.r << "   " << i->second.hex.q << i->second.hex.r << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            
        }   
        clicked=false;
    
    }
    
}

bool Grid::CheckIfHexIsInGrid(int q, int r)
{
    auto it = axialToHex.find(Axial(q,r));
    if(it!=axialToHex.end()){
        std::cout << "HEX IN GRID" << std::endl;
        return true;
    }
    std::cout << "HEX NOT IN GRID" << std::endl;
    return false;
}

bool Grid::CheckIfAnyWarIsInHex(int q, int r)
{
    if(warriors.size()<1){
        std::cout << "NO WARRIORS ON LIST!!!" << std::endl;
        return false;
    }
    if(axialToWar.size()<1){
        std::cout << "NO WARRIORS ON DICT!!!" << std::endl;
        return false;
    }
    auto it = axialToWar.find(Axial(q,r));
    if(it!=axialToWar.end()){
        std::cout << "WARRIOR FOUND" << std::endl;
        return true;
    }
    else{
        std::cout << "WARRIOR NOT FOUND" << std::endl;
        return false;
    }
}

void Grid::AddWarrior(int q, int r)
{
    if(CheckIfHexIsInGrid(q,r) && !CheckIfAnyWarIsInHex(q,r)){
        Warrior war = Warrior(axialToHex[Axial(q,r)]);
        warriors.push_back(war);
        axialToWar[Axial(q,r)] = war;
        std::cout << q << r << war.hex.q << war.hex.r << std::endl;
    }
}

void Grid::Resize(float scale)
{
    radius = radius*scale;
    for(auto i = axialToHex.begin(); i!=axialToHex.end(); i++)
    {
        i->second.x = startX+radius*3/2*i->second.q-radius*3/2*i->second.r;
        i->second.y = startY-(radius*sqrt(3)/2*i->second.q+radius*sqrt(3)/2*i->second.r);
        i->second.a*=scale;
    }
}

void Grid::Move(float dx, float dy)
{
    startX+=dx;
    startY+=dy;
    for(auto i = axialToHex.begin(); i!=axialToHex.end(); i++)
    {
        i->second.x+=dx;
        i->second.y+=dy;
    }
}
