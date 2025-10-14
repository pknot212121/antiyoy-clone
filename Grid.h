#include <vector>
#include "Hexagon.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader_s.h"

/*
A class containg a vector of Hexagons to concatentate all vertices and indices into one
I am not sure if it is an optiml way
*/
class Grid
{

public:
    Grid(std::vector<Hexagon> _hexagons)
    {
        hexagons = _hexagons;
        for(int i=0;i<_hexagons.size();i++)
        {
            int indiceOffset = i*6;
            Hexagon hex = _hexagons[i];
            std::vector<unsigned int> copiedIndices;

            for(int i=0;i<hex.numberOfIndices;i++){
                copiedIndices.push_back(hex.indices[i]+indiceOffset);
            }

            allVertices.insert(allVertices.end(), hex.vertices.begin(), hex.vertices.end());
            allIndices.insert(allIndices.end(),copiedIndices.begin(),copiedIndices.end());
        }
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(VAO);
        std::cout << "SIZE OF VERTICES: "<< sizeof(std::data(allVertices)) << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * allVertices.size(), std::data(allVertices), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*12*hexagons.size(), std::data(allIndices), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0); 

        // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
        // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
        glBindVertexArray(0); 
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        allTriangles = 4 * hexagons.size();
    }

    void Draw()
    {
        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
        //glDrawArrays(GL_TRIANGLES, 0, 6);
        glDrawElements(GL_TRIANGLES, 3*allTriangles, GL_UNSIGNED_INT, 0);
    }

    std::vector<Hexagon> hexagons;
    std::vector<float> allVertices;
    std::vector<unsigned int> allIndices;
    unsigned int VBO, VAO, EBO;
    int allTriangles;
};