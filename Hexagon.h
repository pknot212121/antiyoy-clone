#pragma once
#include <cmath>
#include <list>
#include <vector>
// #include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader.h"
#include "resource_manager.h"


/*
Simple class containing a vector of vertices of a hexagon
And also indices of potential triangles
*/
class Hexagon{

public:

    Hexagon(){}
    Hexagon(float x, float y, float a, glm::vec3 col);

    
    float x;
    float y;
    float a;
    glm::vec3 color;
    int q=0;
    int r=0;
};