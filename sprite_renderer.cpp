#include "sprite_renderer.h"


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

void SpriteRenderer::DrawHexagon(const Hexagon &hex,float size)
{
    glm::vec3 color = glm::vec3(1.0f,0.5f,0.0f);
    if (hex.getResident()==Resident::Water) {
        color = glm::vec3(0.0f,0.0f,1.0f);
    }
    else if (hex.getResident()==Resident::Empty) {
        color = glm::vec3(1.0f,1.0f,1.0f);
    }
    if (hex.getX()%2==0) {
        this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex.getX()*size * 3/4, hex.getY()*size*sqrt(3)/2), glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
    }
    else {
        this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex.getX()*size * 3/4, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4), glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
    }

}



void SpriteRenderer::DrawBoard(Board *board, int width, int height)
{
    for (int i = 0; i < board->getWidth(); i++) {
        for (int j = 0; j < board->getHeight(); j++) {
            this->DrawHexagon(*board->getHexagon(j,i), width / board->getWidth() * sqrt(3)/2 - sqrt(3) / 4 * board->getWidth());
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

    glBindVertexArray(this->quadVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}