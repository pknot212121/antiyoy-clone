#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "texture.h"
#include "shader.h"

#include "resource_manager.h"
#include "board.h"
#include <cstdlib>

struct Point
{
    Point(int x,int y): x(x),y(y){}
    int x;
    int y;
};

inline std::map<Resident,std::string> warriorToTexture = {
    {Resident::Warrior1,"soilder1"},
    {Resident::Warrior1Moved,"soilder1"},
    {Resident::Warrior2,"soilder2"},
    {Resident::Warrior2Moved,"soilder2"},
    {Resident::Warrior3,"placeholder"},
    {Resident::Warrior3Moved,"placeholder"},
    {Resident::Warrior4,"placeholder"},
    {Resident::Warrior4Moved,"placeholder"},
    {Resident::Farm,"placeholder"}
};

class SpriteRenderer
{
public:
    // Constructor (inits shaders/shapes)
    SpriteRenderer(Shader &shader);
    // Destructor
    ~SpriteRenderer();
    // Renders a defined quad textured with given sprite
    void DrawSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f, glm::vec3 color = glm::vec3(1.0f));
    // void DrawHexagon(float x, float y, float a, glm::vec3 col);
    void DrawHexagon(int playerIndex, const ::Hexagon* hex, float size, glm::vec3 color = glm::vec3(1.0f, 0.5f, 0.0f));
    Point CheckWhichHexagon(int x, int y, float size);
    // void DrawWarrior(Hexagon hex,Warrior war);
    void DrawBoard(Board* board, int width, int height, int playerIndex);
    void InitPalette();
    void addToDisplacementX(int dx);
    void addToDisplacementY(int dy);
    void addToResizeMultiplier(double ds, Board* board, float width);
    void setBrightenedHexes(std::vector<Hexagon*> hexes);
    void ClearBrightenedHexes();
    glm::vec2 calculateHexPosition(int gridX, int gridY, float size);

private:
    // Render state
    Shader       shader; 
    unsigned int quadVAO;
    std::vector<glm::vec3> palette;
    int   displacementX = 0;
    int   displacementY = 0;
    double resizeMultiplier = 1.0f;
    std::vector<Hexagon*> brightenedHexes;
    // Initializes and configures the quad's buffer and vertex attributes
    void initRenderData();
};

#endif