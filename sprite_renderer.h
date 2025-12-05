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
    "soilder3",
    "placeholder",
    "soilder1",
    "soilder2",
    "soilder3",
    "placeholder",
    "farm1",
    "castle",
    "tower",
    "strongTower",
    "palm",
    "pine",
    "gravestone"
};



inline std::unordered_set<Resident> active = {Resident::Warrior1,Resident::Warrior2,Resident::Warrior3,Resident::Warrior4};

class SpriteRenderer
{
public:
    SpriteRenderer(Shader &shader, Board *board);
    void getActualDimensions(Board* board);
    ~SpriteRenderer();
    void constrainMapBounds(Board* board);

    std::vector<int> getAllIndicesOnAScreen(Board* board);
    void generateSprites(Board* board);
    void generateBorders(Board* board);
    void DrawBoard(Board* board, int width, int height);


    bool isHexOnScreen(glm::vec2 hexPos);
    float getSize(Board* board);
    void RenderBatch(const std::string& textureName, const std::vector<HexInstanceData>& data);
    glm::ivec2 CheckWhichHexagon(int x, int y, float size);
    glm::ivec2 CheckWhichActualHexagon(int _x, int _y, float baseSize);
    void Zoom(float zoomFactor, float pivotX, float pivotY, Board* board);
    glm::vec2 calculateHexPosition(int gridX, int gridY, float size);
    void setPosToCastle(Board *board,uint8 id);
    void InitPalette();

    void addToDisplacementX(Board *board,int dx);
    void addToDisplacementY(Board *board,int dy);
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
    int   actualBoardWidth=0;
    int   actualBoardHeight=0;
    int actualMinX=0;
    int actualMaxX =0;
    int actualMinY = 0;
    int actualMaxY = 0;
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
    void initRenderData(int bWidth, int bHeight);
};

#endif