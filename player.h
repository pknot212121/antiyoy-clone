#pragma once
#include <glm/glm.hpp>
#include "axial.h"
#include <set>
#include <string>
class Player
{
public:
Player(){}
Player(glm::vec3 _color, std::string name);

glm::vec3 color;
std::set<Axial> hexagons;
std::set<Axial> warriors;
std::string name;
};