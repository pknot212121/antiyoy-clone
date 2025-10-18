#include "Hexagon.h"
#include <iostream>

Hexagon::Hexagon(float _x, float _y, float a, glm::vec3 col)
{
    vertices.push_back(_x+a); vertices.push_back(_y); vertices.push_back(0.0f);
    vertices.push_back(_x+a/2); vertices.push_back(_y+sqrt(3)*a/2); vertices.push_back(0.0f);
    vertices.push_back(_x-a/2); vertices.push_back(_y+sqrt(3)*a/2); vertices.push_back(0.0f);
    vertices.push_back(_x-a); vertices.push_back(_y); vertices.push_back(0.0f);
    vertices.push_back(_x-a/2); vertices.push_back(_y-sqrt(3)*a/2); vertices.push_back(0.0f);
    vertices.push_back(_x+a/2); vertices.push_back(_y-sqrt(3)*a/2); vertices.push_back(0.0f);
    numberOfTriangles=4;
    x = _x;
    y = _y;
    this->a = a;
    numberOfIndices =12;
    color = col;
    
}

