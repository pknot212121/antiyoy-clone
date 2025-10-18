#pragma once
#include <cmath>
#include <list>
#include <vector>
// #include <string>
#include "Color.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader_s.h"


/*
Simple class containing a vector of vertices of a hexagon
And also indices of potential triangles
*/
class Hexagon{

public:

    Hexagon(){}
    Hexagon(float x, float y, float a, std::string col);
    void SaveHexagon();
    void DrawHexagon();

    
    std::vector<float> vertices;
    int numberOfTriangles;
    int numberOfIndices;
    float centerX;
    float centerY;
    float radius;
    Color color;
    int q=0;
    int r=0;
    unsigned int VAO,VBO,EBO;
    Shader outlineShader;

    unsigned int indices[12] = {
    0, 1, 2,   
    2, 3, 4,
    2, 4, 5,
    5, 0, 2,    
        }; 
};