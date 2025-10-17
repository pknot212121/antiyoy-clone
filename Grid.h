#include <vector>
#include "Hexagon.h"

#include "Man.h"

/*
A class containg a vector of Hexagons to concatentate all vertices and indices into one
I am not sure if it is an optiml way
*/

class Grid
{

public:

    Grid() {}    

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

        Hexagon hexInside = Hexagon(hex.centerX,hex.centerY,hex.radius*0.95,"Pink");
        allVerticesInside.insert(allVerticesInside.end(), hexInside.vertices.begin(), hexInside.vertices.end());

        insideShader = Shader("4.2.texture.vs", "4.2.texture.fs");
        outlineShader = Shader("inside.vs","inside.fs");
    }

    void SaveSetup()
    {
        glGenVertexArrays(2, VAOs);
        glGenBuffers(2, VBOs);
        glGenBuffers(2, EBOs);

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
        // hexagons[1].SaveHexagon();
    }

    void Draw()
    {
        float timeValue = glfwGetTime();
        float greenValue = sin(timeValue) / 2.0f + 0.5f;
        glBindVertexArray(VAOs[0]);

        outlineShader.use();
        outlineShader.setVec4f("ourColor",greenValue,0.0f,0.0f,1.0f);
        glDrawElements(GL_TRIANGLES, 3*4 * hexagons.size(), GL_UNSIGNED_INT, 0);

        glBindVertexArray(VAOs[1]);
        insideShader.use();
        glDrawElements(GL_TRIANGLES, 3*4*hexagons.size(), GL_UNSIGNED_INT,0);
        
        if(clicked)
        {
            clickedHex.DrawHexagon();
        }
        

        // glBindVertexArray(ChangeHexColor(hexagons[1]));
        // outlineShader.use();
        // outlineShader.setVec4f("ourColor",greenValue,0.0f,0.0f,1.0f);
        // glDrawElements(GL_TRIANGLES, 3*4, GL_UNSIGNED_INT, 0);

    }

    void AddHexagon(int q,int r)
    { 
        Hexagon hex = Hexagon(centerX+radius*3/2*q-radius*3/2*r,centerY+radius*sqrt(3)/2*q+radius*sqrt(3)/2*r,radius,"Red");
        hex.q = q;
        hex.r = r;
        int indiceOffset = hexagons.size()*6;
        std::vector<unsigned int> copiedIndices;

        for(int i=0;i<hex.numberOfIndices;i++){
            copiedIndices.push_back(hex.indices[i]+indiceOffset);
        }

        allVertices.insert(allVertices.end(), hex.vertices.begin(), hex.vertices.end());
        allIndices.insert(allIndices.end(),copiedIndices.begin(),copiedIndices.end());
        hexagons.push_back(hex);

        Hexagon hexInside = Hexagon(hex.centerX,hex.centerY,hex.radius*0.9,"Pink");
        allVerticesInside.insert(allVerticesInside.end(), hexInside.vertices.begin(), hexInside.vertices.end());
    }

    void CheckWhichHexagon(int x, int y, int SCR_WIDTH, int SCR_HEIGHT)
    {
        float normalisedX = ((float)x - ((float)SCR_WIDTH)/2)/(float)SCR_WIDTH * 2;
        float normalisedY = ((float)y - ((float)SCR_HEIGHT)/2) * (-1) / (float)SCR_HEIGHT * 2;
        float a = 0.3f;
        std::cout << "NORMALIZEDX: " << normalisedX << " NORMALIZEDY: " << normalisedY << std::endl;

        float r = (sqrt(3)*normalisedY-normalisedX) / a / 3;
        float q = (sqrt(3)*normalisedY+normalisedX) / a / 3;

        int r_rounded = (int)r, q_rounded = (int)q;

        if(abs(r-r_rounded)>=0.5){ r_rounded+= (r/abs(r));}
        if(abs(q-q_rounded)>=0.5){ q_rounded+= (q/abs(q));}

        
        std::cout << "R: " << r_rounded << " Q: " << q_rounded << std::endl;
        if(!clicked)
        {
            if(CheckIfHexIsInGrid(q_rounded,r_rounded)){SaveHexagonFromCoords(q_rounded,r_rounded); }
        }
        else {clicked=false;}
        
    }

    bool CheckIfHexIsInGrid(int q, int r)
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

    void SaveHexagonFromCoords(int q, int r)
    {
        for(Hexagon hex : hexagons){
            if(hex.q == q && hex.r == r){
                hex.SaveHexagon();
                clicked = true;
                clickedHex = hex;
                return;
            }
        }
        
    }

    

    std::vector<Hexagon> hexagons;
    std::vector<float> allVertices;
    std::vector<float> allVerticesInside;
    std::vector<unsigned int> allIndices;
    unsigned int VBOs[2], VAOs[2], EBOs[2];
    Shader outlineShader;
    Shader insideShader;
    float centerX;
    float centerY;
    float radius;
    Hexagon clickedHex;
    bool clicked;


    
};