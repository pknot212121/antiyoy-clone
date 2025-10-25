#include "Grid.h"
#include<cmath>
#include<iostream>



Grid::Grid(float _startX, float _startY, float hexRadius)
{
    Hexagon hex = Hexagon(_startX,_startY,hexRadius,glm::vec3(0.5f,0.5f,0.5f));
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
    axialToHex[Axial(q,r)] = hex;
}

void Grid::TryToClickOnHexagon(float x, float y)
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
        
        if(CheckIfHexIsInGrid(q_rounded,r_rounded) && !CheckIfAnyWarIsInHex(q_rounded,r_rounded))
        {
            Axial ax = Axial(q_rounded,r_rounded);
            for (auto i = namesToPlayers.begin(); i != namesToPlayers.end(); i++)
            {
                std::set<Axial>::iterator hexIt = i->second.hexagons.find(ax);
                if(hexIt != i->second.hexagons.end()){
                    i->second.hexagons.erase(ax);
                }
            }
            
            for (auto i = namesToPlayers.begin(); i != namesToPlayers.end(); i++)
            {
                // if(i->second.hexagons.find(ax) != i->second.hexagons.end()){i->second.hexagons.erase(ax);}
                std::set<Axial>::iterator warIt = i->second.warriors.find(Axial(moving.hex.q,moving.hex.r));
                if(warIt != i->second.warriors.end()){
                    i->second.hexagons.insert(ax);
                    i->second.warriors.erase(warIt); 
                    i->second.warriors.insert(ax); 
                }
                


                std::cout<<"PLAYER: " << i->first << std::endl;
                std::cout << "HEXES: " << std::endl;
                for(auto const& j : i->second.hexagons){std::cout << j;}
                std::cout << "WARRIORS: " << std::endl;
                for(auto const& j : i->second.warriors){std::cout << j;}
            }
            Hexagon &hex = axialToHex[Axial(q_rounded,r_rounded)];
            axialToWar.erase(Axial(moving.hex.q,moving.hex.r));
            axialToHex[Axial(q_rounded,r_rounded)].color = moving.hex.color;
            moving.hex = hex;
            axialToWar[Axial(q_rounded,r_rounded)] = moving;
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
    if(axialToWar.size()<1){
        // std::cout << "NO WARRIORS ON DICT!!!" << std::endl;
        return false;
    }
    auto it = axialToWar.find(Axial(q,r));
    if(it!=axialToWar.end()){
        // std::cout << "WARRIOR FOUND" << std::endl;
        return true;
    }
    else{
        // std::cout << "WARRIOR NOT FOUND" << std::endl;
        return false;
    }
}

void Grid::AddPlayer(glm::vec3 color, std::string _name)
{
    namesToPlayers[_name] = Player(color,_name);
}

void Grid::AddHexToPlayer(int q, int r, std::string name)
{  
    if(CheckIfHexIsInGrid(q,r))
    {
        namesToPlayers[name].hexagons.insert(Axial(q,r));
        axialToHex[Axial(q,r)].color = namesToPlayers[name].color;
    }
}

void Grid::AddWarToPlayer(int q, int r, std::string name)
{   
    AddWarrior(Axial(q,r));
    namesToPlayers[name].warriors.insert(Axial(q,r));
    axialToWar[Axial(q,r)].hex.color = namesToPlayers[name].color;
    AddHexToPlayer(q,r,name);
}

void Grid::AddWarToPlayer(Axial ax, std::string name)
{   AddWarrior(ax);
    namesToPlayers[name].warriors.insert(ax);
    axialToWar[ax].hex.color = namesToPlayers[name].color;
    AddHexToPlayer(ax.q,ax.r,name);
}


void Grid::AddWarrior(int q, int r)
{
    if(CheckIfHexIsInGrid(q,r) && !CheckIfAnyWarIsInHex(q,r)){
        Warrior war = Warrior(axialToHex[Axial(q,r)]);
        axialToWar[Axial(q,r)] = war;
        std::cout << q << r << war.hex.q << war.hex.r << std::endl;
    }
}
void Grid::AddWarrior(Axial ax)
{
    if(CheckIfHexIsInGrid(ax.q,ax.r) && !CheckIfAnyWarIsInHex(ax.q,ax.r)){
        Warrior war = Warrior(axialToHex[Axial(ax.q,ax.r)]);
        axialToWar[Axial(ax.q,ax.r)] = war;
        std::cout << ax.q << ax.r << war.hex.q << war.hex.r << std::endl;
    }
}

auto selectRandomMap(const std::map<Axial,Hexagon> &s, size_t n) {
  auto it = std::begin(s);
  // 'advance' the iterator n times
  std::advance(it,n);
  return it->first;
}

Axial Grid::GetRandomHex()
{
    // srand((unsigned) time(NULL));
    auto r = rand() % axialToHex.size();
    Axial n = selectRandomMap(axialToHex, r);
    int i=0;
    while(CheckIfAnyWarIsInHex(n.q,n.r) && i<axialToHex.size())
    {
        r = rand() % axialToHex.size();
        n = selectRandomMap(axialToHex, r);
        i++;
    }
    return n;
}

void Grid::AddWarriorFirst()
{
    for(auto i = axialToHex.begin(); i!=axialToHex.end(); i++)
    {
        Axial ax = i->first;
        if(!CheckIfAnyWarIsInHex(ax.q,ax.r))
        {
            axialToWar[ax] = Warrior(i->second);
            break;
        }
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

template<typename S>
auto select_random(const S &s, size_t n) {
  auto it = std::begin(s);
  // 'advance' the iterator n times
  std::advance(it,n);
  return it;
}

void Grid::GenerateMap(int count)
{
    srand((unsigned) time(NULL));
    std::set<Axial> coords = {Axial(0,0),Axial(1,0),Axial(-1,0),Axial(0,1),Axial(0,-1),Axial(1,1),Axial(-1,-1)};
    std::vector<Axial> chosen = {Axial(0,0)};
    for(int i=0;i<count;i++)
    {
        for(auto ch : chosen){
            coords.erase(ch);
        }
        auto r = rand() % coords.size(); // not _really_ random
        Axial n = *select_random(coords, r);
        chosen.push_back(n);
        AddHexagon(n.q,n.r);
        coords.insert(Axial(n.q+1,n.r));
        coords.insert(Axial(n.q-1,n.r));
        coords.insert(Axial(n.q,n.r+1));
        coords.insert(Axial(n.q,n.r-1));
        coords.insert(Axial(n.q+1,n.r+1));
        coords.insert(Axial(n.q-1,n.r-1));
        // std::cout << "CHOSEN: (" << n.q << " , " << n.r << ")\n";  
    }
}

