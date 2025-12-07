#include "sprite_renderer.h"

#include "game.h"
#include "GLFW/glfw3.h"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"

#include <algorithm>



SpriteRenderer::SpriteRenderer(Shader &shader,Board *board)
{
    int bWidth = board->getWidth();
    int bHeight = board->getHeight();
    this->shader = shader;
    this->initRenderData(bWidth,bHeight);
    this->hexData.resize(bWidth*bHeight);
    this->residentData.resize(20);
    for (auto& r : residentData) r.resize(bWidth*bHeight);
    this->InitPalette(board);
}

void SpriteRenderer::getActualDimensions(Board *board)
{
    int maxX = 0; int minX = board->getWidth()-1;
    int maxY = 0; int minY = board->getHeight()-1;
    for (int i=0;i<board->getWidth()*board->getHeight();i++)
    {
        Hexagon *h = board->getHexagon(i);
        if (!water(h->getResident()))
        {
            if (h->getX()>maxX) maxX=h->getX();
            if (h->getX()<minX) minX=h->getX();
            if (h->getY()>maxY) maxY=h->getY();
            if (h->getY()<minY) minY=h->getY();
        }
    }
    actualMinX = minX;
    actualMaxX = maxX;
    actualMaxY = maxY;
    actualMinY = minY;
    actualBoardWidth=abs(maxX-minX);
    actualBoardHeight=abs(maxY-minY);
    std::cout << actualBoardWidth << " " << actualBoardHeight << std::endl;
}

SpriteRenderer::~SpriteRenderer()
{
    glDeleteVertexArrays(1, &this->quadVAO);
}

void SpriteRenderer::addToDisplacementX(Board *board,int dx)
{
    float displacementMultiX = (float)width/800;

    displacementX += dx*displacementMultiX;
    glm::ivec2 pos1 = CheckWhichHexagon(width,0,size/2);
    glm::ivec2 pos2 = CheckWhichHexagon(0,height,size/2);
    if (pos1.x<actualMinX) displacementX-=2*abs(dx)*displacementMultiX;
    if (pos2.x>actualMaxX) displacementX+=2*abs(dx)*displacementMultiX;

}
void SpriteRenderer::addToDisplacementY(Board *board,int dy)
{
    float displacementMultiY = (float)height/600;
    displacementY += dy*displacementMultiY;
    glm::ivec2 pos1 = CheckWhichHexagon(width,0,size/2);
    glm::ivec2 pos2 = CheckWhichHexagon(0,height,size/2);
    if (pos2.y<actualMinY) displacementY-=2*abs(dy)*displacementMultiY;
    if (pos1.y>actualMaxY) displacementY+=2*abs(dy)*displacementMultiY;
}


void SpriteRenderer::addToResizeMultiplier(double ds,Board *board,float width)
{
    resizeMultiplier *= ds;
}

void SpriteRenderer::setBrightenedHexes(std::vector<Hexagon*> hexes)
{
    for (auto hex : hexes)
    {
        brightenedHexes.push_back(hex);
    }
}

void SpriteRenderer::ClearBrightenedHexes()
{
    brightenedHexes.clear();
}

void SpriteRenderer::InitPalette(Board *board) {
    std::vector<int> hexColors{
        0xCC3333,
        0x33CC33,
        0x3333CC,
        0xCCCC33,
        0x33CCCC,
        0xCC33CC,
        0xCC6633,
        0x99CC33,
        0x3399CC,
        0x9933CC
    };
    std::shuffle(hexColors.begin(),hexColors.end(),board->getGen());
    for (auto hex : hexColors)
    {

        double red, green, blue;
        red = hex >> 16 ;

        green = (hex & 0x00ff00) >> 8;

        blue = (hex & 0x0000ff);
        // std::cout << red << " " << green << " " << blue << std::endl;
        palette.push_back(glm::vec3(red/255.0f,green/255.0f,blue/255.0f));
    }
}

glm::ivec2 fromAxial(int q,int r)
{
    int parity = q&1;
    int col = q;
    int row = r + (q - parity) / 2;
    return glm::ivec2(col, row);
}

glm::ivec2 SpriteRenderer::CheckWhichHexagon(int _x, int _y, float baseSize)
{

    float worldX = _x - this->displacementX;
    float worldY = _y - this->displacementY;

    float normalizedX = worldX;
    float normalizedY = worldY;

    float a = baseSize;

    float x = normalizedX - a;
    float y = normalizedY - 0.866 * a;

    float x2 = x / a;
    float y2 = y / a;

    int q = round(2./3 * x2);
    int r = round(-1./3 * x2 + sqrt(3)/3 * y2);

    return fromAxial(q, r);
}


void SpriteRenderer::Zoom(float zoomFactor, float pivotX, float pivotY,Board *board)
{
    float oldZoom = resizeMultiplier;
    float newZoom = oldZoom * zoomFactor;

    if (newZoom < 0.3f) newZoom = 0.3f;
    if (newZoom > std::max(actualBoardWidth,actualBoardHeight)/4) newZoom = std::max(actualBoardWidth,actualBoardHeight)/4;

    float scaleRatio = newZoom / oldZoom;

    displacementX = pivotX - (pivotX - displacementX) * scaleRatio;
    displacementY = pivotY - (pivotY - displacementY) * scaleRatio;

    resizeMultiplier = newZoom;
}

const float SQRT_3 = 1.7320508f;


glm::vec2 SpriteRenderer::calculateHexPosition(int gridX, int gridY, float size)
{
    float height = size * SQRT_3 / 2.0f;
    float posX = gridX * size * 0.75f + displacementX;
    float posY = gridY * height + displacementY;
    if (gridX % 2 != 0)
    {
        posY += height / 2.0f;
    }
    return glm::vec2(posX, posY);
}

void SpriteRenderer::setPosToCastle(Board* board,uint8 id)
{
    std::unordered_set<Hexagon*> hexes = board->getHexesOfCountry(id);
    Hexagon *h = *hexes.begin();
    glm::vec2 pos = calculateHexPosition(h->getX(),h->getY(),size);
    displacementX -= pos.x;
    displacementX += width/2;
    displacementY-=pos.y;
    displacementY += height/2;
}

glm::vec2 Jump(float size)
{
    float time = glfwGetTime();
    float speed = 3.0f;
    float pulse = (std::sin(time * speed) + 1.0f) / 2.0f * size / 5;
    return glm::vec2(0.0f,pulse);
}
bool SpriteRenderer::isHexOnScreen(glm::vec2 hexPos)
{
    return !(hexPos.x>width || hexPos.x<-size || hexPos.y > height || hexPos.y<-size);
}

float SpriteRenderer::getSize(Board *board)
{
    float boardWidth = static_cast<float>(std::max(actualBoardWidth,actualBoardHeight));
    float screenWidth = static_cast<float>(width) * resizeMultiplier;
    return (screenWidth / boardWidth) / 0.75f;
}

void SpriteRenderer::RenderBatch(const std::string &textureName, const std::vector<HexInstanceData> &data)
{
    if (data.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, this->instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(HexInstanceData), data.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    ResourceManager::GetTexture(textureName).Bind();

    glBindVertexArray(this->quadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, data.size());
    glBindVertexArray(0);
}

std::vector<std::pair<coord, coord>> evenD =
{
    { 0, -1}, // górny
    {-1, -1}, // lewy górny
    {-1,  0}, // lewy dolny
    { 0,  1}, // dolny
    { 1,  0}, // prawy dolny
    { 1, -1}  // prawy górny
};

std::vector<std::pair<coord, coord>> oddD =
{
    { 0, -1}, // górny
    {-1,  0}, // lewy górny
    {-1,  1}, // lewy dolny
    { 0,  1}, // dolny
    { 1,  1}, // prawy dolny
    { 1,  0}  // prawy górny
};

std::vector<glm::vec2> getCenters(float a,glm::vec2 start)
{
    return std::vector<glm::vec2>{
                {glm::vec2(a,0.0f)+start},
                {glm::vec2(0.25*a,0.433*a)+start},
                {glm::vec2(0.25*a,1.299*a)+start},
                {glm::vec2(a,1.732*a)+start},
                {glm::vec2(1.75 *a,1.299*a)+start},
                {glm::vec2(1.75 * a,0.433*a)+start},
            };
}

std::vector<int> SpriteRenderer::getAllIndicesOnAScreen(Board *board)
{
    std::vector<int> ind;
    glm::ivec2 p1 = CheckWhichHexagon(0,0,size);
    if (p1.x<0) p1.x=0; if (p1.y<0) p1.y=0;

    glm::ivec2 p2 = CheckWhichHexagon(width,height+size,size/2);
    if (p2.x>=board->getWidth()) p2.x=board->getWidth()-1; if (p2.y>=board->getHeight()) p2.y=board->getHeight()-1;
    for (int i=p1.y*board->getWidth()+p1.x;i<=p2.y*board->getWidth()+p2.x;i++)
    {
        if (i<board->getWidth()*board->getHeight() && i>=0)
        {
            ind.push_back(i);
        }
    }
    return ind;
}

void SpriteRenderer::generateSprites(Board *board)
{
    size = getSize(board);
    hexData.clear();
    exclamationData.clear();
    shieldData.clear();
    for (auto& r : residentData) r.clear();

    std::vector<int> ind = getAllIndicesOnAScreen(board);
    for (int i : ind)
    {
        Hexagon *hex = board->getHexagon(i);
        glm::vec2 hexSizeVec(size, size * 1.73 / 2.0f);
        float smallSize = size * 0.8;
        glm::vec2 smallSizeVec(smallSize, smallSize);
        glm::vec3 color = glm::vec3(1.0f,1.0f,1.0f);

        if (hex->getOwnerId()!=0) color = palette[hex->getOwnerId()%10];
        if (auto it = std::ranges::find(brightenedHexes,hex);it!=brightenedHexes.end()) color -= glm::vec3(0.2,0.2,0.2);

        glm::vec2 hexPos = calculateHexPosition(hex->getX(), hex->getY(), size);
        glm::vec2 unitPos = hexPos + glm::vec2((size-smallSize)/2,0);

        if (!water(hex->getResident())) hexData.push_back(HexInstanceData(hexPos,color,0.0f,hexSizeVec));
        residentData[(int)hex->getResident()].push_back({unitPos,glm::vec3(1.0f),0.0f,smallSizeVec});
        if (castle(hex->getResident()) && hex->getOwnerId()==board->getCurrentPlayerId()) exclamationData.push_back({unitPos,glm::vec3(1.0f),0.0f,smallSizeVec});
        if (shieldHexes.contains(hex)) shieldData.push_back({unitPos,glm::vec3(1.0f),0.0f,smallSizeVec});

    }
}

void SpriteRenderer::generateBorders(Board *board)
{
    borderData.clear();
    if (board->getGame()->provinceSelector!=nullptr)
    {

        std::vector<Hexagon*> hexes = board->getGame()->provinceSelector->province(board);
        std::vector<float> rotations = {0.0f,120.0f,60.0f,0.0f,120.0f,60.0f};
        for (auto& hex : hexes)
        {
            auto& directions = (hex->getX() % 2 == 0) ? evenD : oddD;
            int i=0;
            for (auto [dx, dy] : directions)
            {
                Hexagon* n = board->getHexagon(hex->getX() + dx, hex->getY() + dy);
                if(n == nullptr || n->getOwnerId()!=board->getCurrentPlayerId())
                {
                    float width = size * 0.07;
                    float a = size/2;
                    glm::vec2 hexSizeVec(size, size * 1.73 / 2.0f);
                    std::vector<glm::vec2> centers = getCenters(a,calculateHexPosition(hex->getX(),hex->getY(),size));
                    glm::vec3 color = palette[hex->getOwnerId()%10];
                    color -= glm::vec3(0.25,0.25,0.25);
                    borderData.push_back({centers[i]-glm::vec2(size/4,0),color,rotations[i],glm::vec2(a,width)});
                }
                i++;
            }
        }
    }
}


void SpriteRenderer::DrawBoard(Board *board, int width, int height)
{
    // This generates sprites and puts them in vectors ready to render
    generateSprites(board);
    generateBorders(board);

    this->shader.Use();
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    this->shader.SetMatrix4("projection", projection);

    RenderBatch("hexagon", hexData);

    RenderBatch("border_placeholder",borderData);
    for (int i=0;i<=(int)Resident::Gravestone;i++)
    {
        if (textures[i]!="nic" )
        {
            if (i>=(int)Resident::Warrior1 && i<=(int)Resident::Warrior4)
            {
                glm::vec2 j = Jump(size/2);
                for (auto& d : residentData[i]) d.position-=j;
            }
            RenderBatch(textures[i],residentData[i]);
        }

    }
    RenderBatch("exclamation",exclamationData);
    RenderBatch("shield",shieldData);

}





void SpriteRenderer::initRenderData(int bWidth,int bHeight)
{
    // 1. Definicja geometrii pojedynczego quada (tak jak miałeś w oryginale)
    float vertices[] = {
        // pos      // tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };

    unsigned int VBO; // To VBO dla geometrii (lokalne, bo dane są statyczne)

    glGenVertexArrays(1, &this->quadVAO);
    glGenBuffers(1, &VBO);

    // --- KONFIGURACJA 1: GEOMETRIA (Quad) ---
    glBindVertexArray(this->quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Atrybut 0: vertex (vec4: x,y,u,v) - czytany z VBO
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // Ważne: NIE ustawiamy tu Divisor, bo to są dane per-wierzchołek!


    glGenBuffers(1, &this->instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, this->instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HexInstanceData) * bWidth * bHeight, nullptr, GL_DYNAMIC_DRAW);

    // Atrybut 1: Position (vec2) - czytany z instanceVBO
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(HexInstanceData), (void*)offsetof(HexInstanceData, position));
    glVertexAttribDivisor(1, 1); // Dane zmieniają się co 1 instancję

    // Atrybut 2: Color (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(HexInstanceData), (void*)offsetof(HexInstanceData, color));
    glVertexAttribDivisor(2, 1);

    // Atrybut 3: Rotation (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(HexInstanceData), (void*)offsetof(HexInstanceData, rotation));
    glVertexAttribDivisor(3, 1);

    // Atrybut 4: Size (vec2)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(HexInstanceData), (void*)offsetof(HexInstanceData, size));
    glVertexAttribDivisor(4, 1);

    // Sprzątanie
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


}
