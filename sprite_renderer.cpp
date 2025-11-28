#include "sprite_renderer.h"

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


void SpriteRenderer::DrawHexagon(int playerIndex,const Hexagon* hex, float size, glm::vec3 color)
{
    size *= resizeMultiplier;
    float smallSize = size * 0.8;
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



    if (hex->getResident()!=Resident::Water) {
        if (hex->getX()%2==0) {
            this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex->getX()*size * 3/4 + displacementX, hex->getY()*size*sqrt(3)/2+ displacementY) , glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
            if (warriorToTexture.contains(hex->getResident()))
            {
                this->DrawSprite(ResourceManager::GetTexture(warriorToTexture[hex->getResident()]),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 - smallSize/2 + displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
            else if (hex->getResident()==Resident::Castle)
            {
                this->DrawSprite(ResourceManager::GetTexture("castle"),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 - smallSize /2 + displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
                if (hex->getOwnerId()==playerIndex)
                {
                    this->DrawSprite(ResourceManager::GetTexture("exclamation"),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 - smallSize/2 + displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
                }
            }
        }
        else {
            this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex->getX()*size * 3/4 + displacementX, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + displacementY), glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
            if (warriorToTexture.contains(hex->getResident()))
            {
                this->DrawSprite(ResourceManager::GetTexture(warriorToTexture[hex->getResident()]),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + size*sqrt(3)/4 - smallSize/2 + displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
            else if (hex->getResident()==Resident::Castle)
            {
                this->DrawSprite(ResourceManager::GetTexture("castle"),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + size*sqrt(3)/4 - smallSize/2 + displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
                if (hex->getOwnerId()==playerIndex)
                {
                    this->DrawSprite(ResourceManager::GetTexture("exclamation"),glm::vec2(hex->getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex->getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + size*sqrt(3)/4 - smallSize/2+ displacementY), glm::vec2(smallSize,smallSize), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
                }
            }
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