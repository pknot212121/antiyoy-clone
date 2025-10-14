#pragma once
#include <cmath>
#include <list>
#include <vector>


/*
Simple class containing a vector of vertices of a hexagon
And also indices of potential triangles
*/
class Hexagon{

public:
    Hexagon(float x, float y, float a){
        vertices.push_back(x+a); vertices.push_back(y); vertices.push_back(0.0f);
        vertices.push_back(x+a/2); vertices.push_back(y+sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x-a/2); vertices.push_back(y+sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x-a); vertices.push_back(y); vertices.push_back(0.0f);
        vertices.push_back(x-a/2); vertices.push_back(y-sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x+a/2); vertices.push_back(y-sqrt(3)*a/2); vertices.push_back(0.0f);
        numberOfTriangles=4;
        centerX = x;
        centerY = y;
        numberOfIndices =12;
    }
    
    std::vector<float> vertices;
    int numberOfTriangles;
    int numberOfIndices;
    int centerX;
    int centerY;

    unsigned int indices[12] = {
    0, 1, 2,   
    2, 3, 4,
    2, 4, 5,
    5, 0, 2,    
        }; 
};