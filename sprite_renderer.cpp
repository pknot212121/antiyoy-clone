#include "sprite_renderer.h"

#include "GLFW/glfw3.h"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"
#include "glm/common.hpp"


SpriteRenderer::SpriteRenderer(Shader &shader)
{
    this->shader = shader;
    this->initRenderData();
}

SpriteRenderer::~SpriteRenderer()
{
    glDeleteVertexArrays(1, &this->quadVAO);
}

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
    srand(time(NULL));
    std::vector<int> temp_vector;
    int reps = base_vector.size();

    //Pull a random element from base_vector, add it to temp_vector, then delete it from base_vector
    for (int i = 0; i < reps; i++)
    {
        int random_number = rand() % base_vector.size();
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

    float currentSize = baseSize * this->resizeMultiplier;
    float normalizedX = worldX / this->resizeMultiplier;
    float normalizedY = worldY / this->resizeMultiplier;

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
    if (newZoom > 3.0f) newZoom = 3.0f;

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

glm::vec2 Jump(float size)
{
    float time = glfwGetTime();
    float speed = 3.0f;
    float pulse = (std::sin(time * speed) + 1.0f) / 2.0f * size / 5;
    return glm::vec2(0.0f,pulse);
}

void SpriteRenderer::DrawHexagon(int playerIndex, ::Hexagon* const hex, float size, glm::vec3 color)
{
    size *= resizeMultiplier;
    float smallSize = size * 0.8;
    glm::vec2 hexSizeVec(size, size * SQRT_3 / 2.0f);
    glm::vec2 smallSizeVec(smallSize, smallSize);
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
    if (hex->getResident() != Resident::Water)
    {
        this->DrawSprite(ResourceManager::GetTexture("hexagon"), hexPos, hexSizeVec, 0.0f, color);
        glm::vec2 unitPos = hexPos + (hexSizeVec * 0.5f) - (smallSizeVec * 0.5f);

        std::string textureName = "";
        if (warriorToTexture.contains(hex->getResident()))
        {
            textureName = warriorToTexture[hex->getResident()];

        }
        else if (hex->getResident() == Resident::Castle)
        {
            textureName = "castle";
        }
        if (active.contains(hex->getResident()))
        {
            unitPos-=Jump(size);
        }
        if (!textureName.empty())
        {
            this->DrawSprite(ResourceManager::GetTexture(textureName), unitPos, smallSizeVec);
            if (hex->getResident() == Resident::Castle && hex->getOwnerId() == playerIndex)
            {
                this->DrawSprite(ResourceManager::GetTexture("exclamation"), unitPos, smallSizeVec);
            }

        }
        if (hex->getOwnerId() == playerIndex && shieldHexes.contains(hex))
        {
            this->DrawSprite(ResourceManager::GetTexture("shield_placeholder"),unitPos,smallSizeVec);
        }
    }
}



void SpriteRenderer::DrawBoard(Board *board, int width, int height, int playerIndex)
{
    // std::cout << "Width: " << width << " " << "Height: " << height << std::endl;
    for (int i = 0; i < board->getWidth(); i++) {
        for (int j = 0; j < board->getHeight(); j++) {
            this->DrawHexagon(playerIndex,board->getHexagon(j,i), width / board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * board->getWidth());
        }
    }

    // this -> DrawHexagon(Hexagon(100,100), width / board->getWidth());
}





void SpriteRenderer::initRenderData()
{
    // configure VAO/VBO
    unsigned int VBO;
    float vertices[] = { 
        // pos      // tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &this->quadVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    this -> InitPalette();
    glBindVertexArray(this->quadVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}