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

struct HexInstanceData {
    glm::vec2 position;
    glm::vec3 color;
    float rotation;
    glm::vec2 size;
};

inline std::string textures[] = {
    "nic",
    "nic",
    "soilder1",
    "soilder2",
    "placeholder",
    "placeholder",
    "soilder1",
    "soilder2",
    "placeholder",
    "placeholder",
    "placeholder",
    "castle",
    "tower",
    "placeholder",
    "palm",
    "pine",
    "gravestone"
};



inline std::unordered_set<Resident> active = {Resident::Warrior1,Resident::Warrior2,Resident::Warrior3,Resident::Warrior4};

class SpriteRenderer
{
public:
    SpriteRenderer(Shader &shader, int bWidth, int bHeight);
    ~SpriteRenderer();


    void DrawSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f, glm::vec3 color = glm::vec3(1.0f));
    void DrawBoardSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f, glm::vec3 color = glm::vec3(1.0f));
    void DrawHexSprite(glm::vec2 position, glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f, glm::vec3 color = glm::vec3(1.0f));

    void DrawHexagon(int playerIndex, ::Hexagon* hex, float size, glm::vec3 color = glm::vec3(1.0f, 0.5f, 0.0f));
    void DrawResident(::Hexagon* const hex, float size,Resident r, glm::vec3 color = glm::vec3(1.0f));
    void DrawMarker(int playerIndex, ::Hexagon* hex, float size, glm::vec3 color = glm::vec3(1.0f));

    void DrawBoard(Board* board, int width, int height, int playerIndex);
    void DrawBorder(float size, glm::vec3 color, Hexagon* hex, int index, float width, float rotation);
    void DrawOutline(Board* board, float size, uint8 id, Hexagon* h);


    bool isHexOnScreen(glm::vec2 hexPos);
    float getSize(Board* board);
    void RenderBatch(const std::string& textureName, const std::vector<HexInstanceData>& data);
    Point CheckWhichHexagon(int x, int y, float size);
    void Zoom(float zoomFactor, float pivotX, float pivotY);
    glm::vec2 calculateHexPosition(int gridX, int gridY, float size);
    void InitPalette();

    void addToDisplacementX(int dx);
    void addToDisplacementY(int dy);
    void addToResizeMultiplier(double ds, Board* board, float width);
    void setBrightenedHexes(std::vector<Hexagon*> hexes);
    void ClearBrightenedHexes();

    std::unordered_set<Hexagon*> shieldHexes;
    float size;
    std::vector<HexInstanceData> hexData;
    std::vector<HexInstanceData> exclamationData;
    std::vector<HexInstanceData> shieldData;
    std::vector<HexInstanceData> borderData;
    std::vector<std::vector<HexInstanceData>> residentData;
    std::vector<glm::vec3> palette;
    int   displacementX = 0;
    int   displacementY = 0;
    double resizeMultiplier = 1.0f;
    int width;
    int height;
    std::vector<Hexagon*> brightenedHexes;
private:
    // Render state
    Shader       shader; 
    unsigned int quadVAO;
    unsigned int instanceVBO;



    // Initializes and configures the quad's buffer and vertex attributes
    void initRenderData();
};

#endif