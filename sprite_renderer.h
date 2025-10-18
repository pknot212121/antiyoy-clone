#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include "Grid.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "texture.h"
#include "shader.h"

#include "resource_manager.h"
#include "Hexagon.h"
#include "warrior.h"


class SpriteRenderer
{
public:
    // Constructor (inits shaders/shapes)
    SpriteRenderer(Shader &shader);
    // Destructor
    ~SpriteRenderer();
    // Renders a defined quad textured with given sprite
    void DrawSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f, glm::vec3 color = glm::vec3(1.0f));
    void DrawHexagon(float x, float y, float a, glm::vec3 col);
    void DrawHexagon(Hexagon hex);
    void DrawWarrior(Warrior war);
    void DrawGrid(Grid grid);

private:
    // Render state
    Shader       shader; 
    unsigned int quadVAO;
    // Initializes and configures the quad's buffer and vertex attributes
    void initRenderData();
};

#endif