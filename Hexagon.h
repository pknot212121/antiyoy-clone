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

    Hexagon(float x, float y, float a, std::string col){
        vertices.push_back(x+a); vertices.push_back(y); vertices.push_back(0.0f);
        vertices.push_back(x+a/2); vertices.push_back(y+sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x-a/2); vertices.push_back(y+sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x-a); vertices.push_back(y); vertices.push_back(0.0f);
        vertices.push_back(x-a/2); vertices.push_back(y-sqrt(3)*a/2); vertices.push_back(0.0f);
        vertices.push_back(x+a/2); vertices.push_back(y-sqrt(3)*a/2); vertices.push_back(0.0f);
        numberOfTriangles=4;
        centerX = x;
        centerY = y;
        radius = a;
        numberOfIndices =12;
        color = Color(col);
        outlineShader = Shader("inside.vs","flashing.fs");
    }

    void SaveHexagon(){
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 6, std::data(vertices), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*12, std::data(indices), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0); 
    }

    void DrawHexagon()
    {
        float timeValue = glfwGetTime();
        float greenValue = sin(timeValue) / 2.0f + 0.5f;
        glBindVertexArray(VAO);
        outlineShader.use();
        outlineShader.setVec4f("ourColor",greenValue,greenValue,greenValue,1.0f);
        glDrawElements(GL_TRIANGLES, 3*4, GL_UNSIGNED_INT, 0);
    }

    
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