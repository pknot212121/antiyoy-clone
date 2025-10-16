#include "Hexagon.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader_s.h"
class Man {
public:
    Man(Hexagon hex)
    {
        posHex = hex;
        height = hex.radius/3;
        width = hex.radius/3;
        vertices.push_back(hex.centerX-width/2); vertices.push_back(hex.centerY-height/2); vertices.push_back(0.0f);
        vertices.push_back(hex.centerX+width/2); vertices.push_back(hex.centerY-height/2); vertices.push_back(0.0f);
        vertices.push_back(hex.centerX+width/2); vertices.push_back(hex.centerY+height/2); vertices.push_back(0.0f);
        vertices.push_back(hex.centerX-width/2); vertices.push_back(hex.centerY+height/2); vertices.push_back(0.0f);
        manShader = Shader("inside.vs", "inside.fs");
    }

    void SaveMan() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(VAO);
        // std::cout << "SIZE OF VERTICES: "<< sizeof(std::data(allVertices)) << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * vertices.size(), std::data(vertices), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*6, indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        // glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0);
    }

    void DrawMan()
    {
        glBindVertexArray(VAO);
        manShader.use();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,0);
    }


    Hexagon posHex;
    float height;
    float width;
    std::vector<float> vertices;

    unsigned int VAO, VBO, EBO;
    Shader manShader;
    unsigned int indices[6] = {
        0, 1, 2,
        2, 0, 3
    };
    
};