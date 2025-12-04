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



SpriteRenderer::SpriteRenderer(Shader &shader,int bWidth, int bHeight)
{
    this->shader = shader;
    this->initRenderData();
    this->hexData.resize(bWidth*bHeight);
    this->residentData.resize(20);
    for (auto& r : residentData) r.resize(bWidth*bHeight);
}

SpriteRenderer::~SpriteRenderer()
{
    glDeleteVertexArrays(1, &this->quadVAO);
}



void SpriteRenderer::addToDisplacementX(int dx)
{
    displacementX += dx;
}
void SpriteRenderer::addToDisplacementY(int dy)
{
    displacementY += dy;
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


std::vector<int> rand_vect(std::vector<int> base_vector)
{
    std::vector<int> temp_vector;
    int reps = base_vector.size();

    for (int i = 0; i < reps; i++)
    {
        std::uniform_int_distribution<> dis(0, base_vector.size() - 1);
        int random_number = dis(gen);

        temp_vector.push_back(base_vector[random_number]);
        base_vector.erase(base_vector.begin() + random_number);
    }
    return temp_vector;
}

void SpriteRenderer::InitPalette() {
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
    std::vector<int> shuffled = rand_vect(hexColors);
    for (auto hex : shuffled)
    {

        double red, green, blue;
        red = hex >> 16 ;

        green = (hex & 0x00ff00) >> 8;

        blue = (hex & 0x0000ff);
        // std::cout << red << " " << green << " " << blue << std::endl;
        palette.push_back(glm::vec3(red/255.0f,green/255.0f,blue/255.0f));
    }
}

Point fromAxial(int q,int r)
{
    int parity = q&1;
    int col = q;
    int row = r + (q - parity) / 2;
    return Point(col, row);
}

Point SpriteRenderer::CheckWhichHexagon(int _x, int _y, float baseSize)
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

void SpriteRenderer::Zoom(float zoomFactor, float pivotX, float pivotY)
{
    float oldZoom = resizeMultiplier;
    float newZoom = oldZoom * zoomFactor;

    if (newZoom < 0.5f) newZoom = 0.5f;
    // if (newZoom > 3.0f) newZoom = 3.0f;

    float scaleRatio = newZoom / oldZoom;
    displacementX = pivotX - (pivotX - displacementX) * scaleRatio;
    displacementY = pivotY - (pivotY - displacementY) * scaleRatio;

    resizeMultiplier = newZoom;
}

const float SQRT_3 = 1.7320508f;


glm::vec2 SpriteRenderer::calculateHexPosition(int gridX, int gridY, float size)
{
    float height = size * SQRT_3 / 2.0f;
    float posX = gridX * size * 0.75f + displacementX / resizeMultiplier;
    float posY = gridY * height + displacementY / resizeMultiplier;
    if (gridX % 2 != 0)
    {
        posY += height / 2.0f;
    }
    return glm::vec2(posX, posY);
}

glm::vec2 Jump(float size)
{
    float time = glfwGetTime();
    float speed = 3.0f;
    float pulse = (std::sin(time * speed) + 1.0f) / 2.0f * size / 5;
    return glm::vec2(0.0f,pulse);
}

// std::vector<glm::vec2> getCenters(float a,glm::vec2 start)
// {
//     return std::vector<glm::vec2>{
//         {glm::vec2(a,0.0f)+start},
//         {glm::vec2(0.25*a,0.433*a)+start},
//         {glm::vec2(0.25*a,1.299*a)+start},
//         {glm::vec2(a,1.732*a)+start},
//         {glm::vec2(1.75 *a,1.299*a)+start},
//         {glm::vec2(1.75 * a,0.433*a)+start},
//     };
// }

// void SpriteRenderer::DrawBorder(float size,glm::vec3 color, Hexagon* hex, int index,float width,float rotation)
// {
//     width = size * 0.07;
//     float a = size/2;
//     std::vector<glm::vec2> centers = getCenters(a,calculateHexPosition(hex->getX(),hex->getY(),size));
//     for (auto& center : centers) center-=glm::vec2(a/2,width/2);
//     color = palette[hex->getOwnerId()%10];
//     color -= glm::vec3(0.25,0.25,0.25);
//     this->DrawBoardSprite(ResourceManager::GetTexture("border_placeholder"),centers[index],glm::vec2(a,width),rotation,color);
// }


void SpriteRenderer::DrawSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size, float rotate, glm::vec3 color)
{
    // prepare transformations
    this->shader.Use();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position, 0.0f));  // first translate (transformations are: scale happens first, then rotation, and then final translation happens; reversed order)

    model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f)); // move origin of rotation to center of quad
    model = glm::rotate(model, glm::radians(rotate), glm::vec3(0.0f, 0.0f, 1.0f)); // then rotate
    model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f)); // move origin back

    model = glm::scale(model, glm::vec3(size, 1.0f)); // last scale

    this->shader.SetMatrix4("model", model);

    // render textured quad
    this->shader.SetVector3f("spriteColor", color);

    glActiveTexture(GL_TEXTURE0);
    texture.Bind();

    glBindVertexArray(this->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SpriteRenderer::DrawBoardSprite(Texture2D &texture, glm::vec2 position, glm::vec2 size, float rotate, glm::vec3 color)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position, 0.0f));  // first translate (transformations are: scale happens first, then rotation, and then final translation happens; reversed order)

    model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f)); // move origin of rotation to center of quad
    model = glm::rotate(model, glm::radians(rotate), glm::vec3(0.0f, 0.0f, 1.0f)); // then rotate
    model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f)); // move origin back

    model = glm::scale(model, glm::vec3(size, 1.0f)); // last scale

    this->shader.SetMatrix4("model", model);

    // render textured quad
    this->shader.SetVector3f("spriteColor", color);

    glActiveTexture(GL_TEXTURE0);
    texture.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
}


void SpriteRenderer::DrawHexSprite(glm::vec2 position, glm::vec2 size, float rotate, glm::vec3 color)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position, 0.0f));  // first translate (transformations are: scale happens first, then rotation, and then final translation happens; reversed order)

    model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f)); // move origin of rotation to center of quad
    model = glm::rotate(model, glm::radians(rotate), glm::vec3(0.0f, 0.0f, 1.0f)); // then rotate
    model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f)); // move origin back

    model = glm::scale(model, glm::vec3(size, 1.0f)); // last scale

    this->shader.SetMatrix4("model", model);

    this->shader.SetVector3f("spriteColor", color);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}



bool SpriteRenderer::isHexOnScreen(glm::vec2 hexPos)
{
    return !(hexPos.x>width || hexPos.x<-size || hexPos.y > height || hexPos.y<-size);
}

void SpriteRenderer::DrawHexagon(int playerIndex, ::Hexagon* const hex, float size, glm::vec3 color)
{
    glm::vec2 hexSizeVec(size, size * SQRT_3 / 2.0f);
    color = glm::vec3(1.0f,1.0f,1.0f);

    if (hex->getOwnerId()!=0) {
        color = palette[hex->getOwnerId()%10];
    }
    if (!brightenedHexes.empty())
    {
        if (auto it = std::ranges::find(brightenedHexes,hex);it!=brightenedHexes.end())
        {
            color -= glm::vec3(0.2,0.2,0.2);
        }
    }
    glm::vec2 hexPos = calculateHexPosition(hex->getX(), hex->getY(), size);
    if (hex->getResident() != Resident::Water && isHexOnScreen(hexPos))
    {
        // this->DrawBoardSprite( ResourceManager::GetTexture("hexagon"),hexPos, hexSizeVec, 0.0f, color);
        this->DrawHexSprite(hexPos,hexSizeVec,0.0f,color);
    }
}

void SpriteRenderer::DrawResident(::Hexagon* const hex, float size,Resident r, glm::vec3 color)
{
    float smallSize = size * 0.8;
    glm::vec2 hexSizeVec(size, size * SQRT_3 / 2.0f);
    glm::vec2 smallSizeVec(smallSize, smallSize);
    glm::vec2 hexPos = calculateHexPosition(hex->getX(), hex->getY(), size);
    glm::vec2 unitPos = hexPos + (hexSizeVec * 0.5f) - (smallSizeVec * 0.5f);

    if (active.contains(r)) unitPos-=Jump(size);
    if (isHexOnScreen(hexPos)) this->DrawHexSprite(unitPos,smallSizeVec);
}

void SpriteRenderer::DrawMarker(int playerIndex,::Hexagon* const hex, float size, glm::vec3 color)
{
    float smallSize = size * 0.8;
    glm::vec2 hexSizeVec(size, size * SQRT_3 / 2.0f);
    glm::vec2 smallSizeVec(smallSize, smallSize);
    glm::vec2 hexPos = calculateHexPosition(hex->getX(), hex->getY(), size);
    glm::vec2 unitPos = hexPos + (hexSizeVec * 0.5f) - (smallSizeVec * 0.5f);

    if (hex->getResident() == Resident::Castle && hex->getOwnerId() == playerIndex) this->DrawBoardSprite(ResourceManager::GetTexture("exclamation"), unitPos, smallSizeVec);
    if (hex->getOwnerId() == playerIndex && shieldHexes.contains(hex)) this->DrawBoardSprite(ResourceManager::GetTexture("shield_placeholder"),unitPos,smallSizeVec);
}

float SpriteRenderer::getSize(Board *board)
{
    float boardWidth = static_cast<float>(board->getWidth());
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


void SpriteRenderer::DrawBoard(Board *board, int width, int height, int playerIndex)
{

    this->shader.Use();
    // Ustaw projekcję
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    this->shader.SetMatrix4("projection", projection);

    // Prześlij parametry kamery (zmieniają się co klatkę)
    this->shader.SetVector2f("viewOffset", glm::vec2(displacementX, displacementY));
    this->shader.SetFloat("zoom", resizeMultiplier);

    // Rysuj (dane w buforze są stałe, GPU robi resztę)
    RenderBatch("hexagon", hexData);

    RenderBatch("border_placeholder",borderData);
    for (auto& r : warriorToTexture)
    {
        if (unmovedWarrior(r.first))
        {
            glm::vec2 j = Jump(getSize(board));
            for (auto& d : residentData[(int)r.first]) d.position-=j;
        }
        RenderBatch(r.second,residentData[(int)r.first]);
    }
    RenderBatch("exclamation",exclamationData);
    RenderBatch("shield",shieldData);
    // this->width = width;
    // this->height = height;
    // this->size = getSize(board);
    // std::vector<HexInstanceData> instanceData;
    // instanceData.reserve(board->getHeight()*board->getWidth());
    //
    // for (int i = 0; i < board->getWidth(); i++) {
    //     for (int j = 0; j < board->getHeight(); j++) {
    //         Hexagon* hex = board->getHexagon(j, i);
    //
    //         glm::vec2 pos = calculateHexPosition(hex->getX(), hex->getY(), size);
    //
    //         glm::vec3 color = palette[hex->getOwnerId() % 10];
    //         if (hex->getOwnerId()==0){color = glm::vec3(1.0f);}
    //         if (isHexOnScreen(pos) && !water(hex->getResident())) instanceData.push_back({pos, color,0.0f,glm::vec2(size,size * SQRT_3 / 2.0f)});
    //     }
    // }
    //
    // glBindBuffer(GL_ARRAY_BUFFER, this->instanceVBO);
    // glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(HexInstanceData), instanceData.data(), GL_DYNAMIC_DRAW);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    //
    // this->shader.Use();
    // glActiveTexture(GL_TEXTURE0);
    // ResourceManager::GetTexture("hexagon").Bind();
    //
    // glBindVertexArray(this->quadVAO);
    // glDrawArraysInstanced(GL_TRIANGLES, 0, 6,instanceData.size());
    // glBindVertexArray(0);
    //
    // // if (board->getGame()->provinceSelector!=nullptr)
    // // {
    // //     glBindVertexArray(this->quadVAO);
    // //     DrawOutline(board,size,board->getCurrentPlayerId(),board->getGame()->provinceSelector);
    // //     glBindVertexArray(0);
    // // }
    // //
    // // glBindVertexArray(this->quadVAO);
    //
    // instanceData.clear();
    // for (int i = 0; i < board->getWidth(); i++) {
    //     for (int j = 0; j < board->getHeight(); j++) {
    //         Hexagon* hex = board->getHexagon(j, i);
    //
    //         glm::vec2 pos = calculateHexPosition(hex->getX(), hex->getY(), size);
    //
    //         glm::vec3 color = glm::vec3(1.0f);
    //         if (hex->getOwnerId()==0){color = glm::vec3(1.0f);}
    //         if (isHexOnScreen(pos) && hex->getResident()==Resident::PalmTree) instanceData.push_back({pos, color,0.0f,glm::vec2(size,size * SQRT_3 / 2.0f)});
    //     }
    // }
    //
    // glBindBuffer(GL_ARRAY_BUFFER, this->instanceVBO);
    // glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(HexInstanceData), instanceData.data(), GL_DYNAMIC_DRAW);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    //
    // this->shader.Use();
    // glActiveTexture(GL_TEXTURE0);
    // ResourceManager::GetTexture("palm").Bind();
    //
    // glBindVertexArray(this->quadVAO);
    // glDrawArraysInstanced(GL_TRIANGLES, 0, 6,instanceData.size());
    // glBindVertexArray(0);
    //
    //
    //
    //
    // instanceData.clear();
    // for (int i = 0; i < board->getWidth(); i++) {
    //     for (int j = 0; j < board->getHeight(); j++) {
    //         Hexagon* hex = board->getHexagon(j, i);
    //
    //         glm::vec2 pos = calculateHexPosition(hex->getX(), hex->getY(), size);
    //
    //         glm::vec3 color = glm::vec3(1.0f);
    //         if (hex->getOwnerId()==0){color = glm::vec3(1.0f);}
    //         if (isHexOnScreen(pos) && hex->getResident()==Resident::PineTree) instanceData.push_back({pos, color,0.0f,glm::vec2(size,size * SQRT_3 / 2.0f)});
    //     }
    // }
    //
    // glBindBuffer(GL_ARRAY_BUFFER, this->instanceVBO);
    // glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(HexInstanceData), instanceData.data(), GL_DYNAMIC_DRAW);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    //
    // this->shader.Use();
    // glActiveTexture(GL_TEXTURE0);
    // ResourceManager::GetTexture("pine").Bind();
    //
    // glBindVertexArray(this->quadVAO);
    // glDrawArraysInstanced(GL_TRIANGLES, 0, 6,instanceData.size());
    // glBindVertexArray(0);
    //
    // glBindVertexArray(0);
    // glBindVertexArray(this->quadVAO);
    // for (int i = 0; i < board->getWidth(); i++) {
    //     for (int j = 0; j < board->getHeight(); j++) {
    //         this->DrawMarker(playerIndex,board->getHexagon(j,i),size);
    //     }
    // }
    // glBindVertexArray(0);
}





void SpriteRenderer::initRenderData()
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(HexInstanceData) * 70000, nullptr, GL_DYNAMIC_DRAW);

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

    this->InitPalette();
}
