#include <vector>
#include "Hexagon.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Man.h"

/*
A class containg a vector of Hexagons to concatentate all vertices and indices into one
I am not sure if it is an optiml way
*/
class Grid
{

public:
    Grid(Hexagon hex)
    {
        hexagons.push_back(hex);
        centerX = hex.centerX;
        centerY = hex.centerY;
        radius = hex.radius;
        std::vector<unsigned int> copiedIndices;
        for(int i=0;i<hex.numberOfIndices;i++){
            copiedIndices.push_back(hex.indices[i]);
        }
        allVertices.insert(allVertices.end(), hex.vertices.begin(), hex.vertices.end());
        allIndices.insert(allIndices.end(),copiedIndices.begin(),copiedIndices.end());

        Hexagon hexInside = Hexagon(hex.centerX,hex.centerY,hex.radius*0.95);
        allVerticesInside.insert(allVerticesInside.end(), hexInside.vertices.begin(), hexInside.vertices.end());

        insideShader = Shader("4.2.texture.vs", "4.2.texture.fs");
        ourShader = Shader("inside.vs","flashing.fs");
    }

    void SaveSetup()
    {
        glGenVertexArrays(2, VAOs);
        glGenBuffers(2, VBOs);
        glGenBuffers(2, EBOs);

        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(VAOs[0]);
        // std::cout << "SIZE OF VERTICES: "<< sizeof(std::data(allVertices)) << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * allVertices.size(), std::data(allVertices), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[0]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*12*hexagons.size(), std::data(allIndices), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        // glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0);

        glBindVertexArray(VAOs[1]);
        glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * allVerticesInside.size(), std::data(allVerticesInside), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[1]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*12*hexagons.size(), std::data(allIndices), GL_STATIC_DRAW);

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE, 3*sizeof(float), (void*)0);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0); 
    }

    void Draw()
    {
        glBindVertexArray(VAOs[0]); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
        //glDrawArrays(GL_TRIANGLES, 0, 6);
        ourShader.use();
        glDrawElements(GL_TRIANGLES, 3*4 * hexagons.size(), GL_UNSIGNED_INT, 0);

        glBindVertexArray(VAOs[1]);
        insideShader.setFloat("uFlashSpeed",10.0f);
        insideShader.setVec4("uFlashColour",0.5f);
        insideShader.use();
        glDrawElements(GL_TRIANGLES, 3*4*hexagons.size(), GL_UNSIGNED_INT,0);
        // for(int i=0;i<hexagons.size();i++)
        // {
        //     glBindVertexArray(VAOs[i]);
        //     glMultiDrawArrays(GL_LINES,0,)
        // }
    }

    void AddHexagon(int q,int r)
    {
        // std::cout << "CENTERX: " << centerX << " , CENTERY: " << centerY << " , RADIUS: " << radius << std::endl; 
        Hexagon hex = Hexagon(centerX+radius*3/2*q-radius*3/2*r,centerY+radius*sqrt(3)/2*q+radius*sqrt(3)/2*r,radius);
        int indiceOffset = hexagons.size()*6;
        std::vector<unsigned int> copiedIndices;

        for(int i=0;i<hex.numberOfIndices;i++){
            copiedIndices.push_back(hex.indices[i]+indiceOffset);
        }

        allVertices.insert(allVertices.end(), hex.vertices.begin(), hex.vertices.end());
        allIndices.insert(allIndices.end(),copiedIndices.begin(),copiedIndices.end());
        hexagons.push_back(hex);

        Hexagon hexInside = Hexagon(hex.centerX,hex.centerY,hex.radius*0.9);
        allVerticesInside.insert(allVerticesInside.end(), hexInside.vertices.begin(), hexInside.vertices.end());
    }

    void CheckWhichHexagon(int x, int y, int SCR_WIDTH, int SCR_HEIGHT)
    {
        float normalisedX = ((float)x - ((float)SCR_WIDTH)/2)/(float)SCR_WIDTH * 2;
        float normalisedY = ((float)y - ((float)SCR_HEIGHT)/2) * (-1) / (float)SCR_HEIGHT * 2;
        float a = hexagons[0].radius;
        std::cout << "NORMALIZEDX: " << normalisedX << " NORMALIZEDY: " << normalisedY << std::endl;

        float r = (normalisedY / (3/2) - normalisedX / (sqrt(3)/2)) / a;
        float q = (normalisedY / (3/2) + normalisedX / (sqrt(3)/2)) / a;
        std::cout << "R: " << r << " Q: " << q << std::endl;
        float stampx = centerX+radius*3/2*q-radius*3/2*r;
    }




    std::vector<Hexagon> hexagons;
    std::vector<float> allVertices;
    std::vector<float> allVerticesInside;
    std::vector<unsigned int> allIndices;
    unsigned int VBOs[2], VAOs[2], EBOs[2];
    Shader ourShader;
    Shader insideShader;
    float centerX;
    float centerY;
    float radius;


    
};