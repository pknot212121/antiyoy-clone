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
    0x000000,
    0x00FF00,
    0x0000FF,
    0xFF0000,
    0x01FFFE,
    0xFFA6FE,
    0xFFDB66,
    0x006401,
    0x010067,
    0x95003A,
    0x007DB5,
    0xFF00F6,
    0xFFEEE8,
    0x774D00,
    0x90FB92,
    0x0076FF,
    0xD5FF00,
    0xFF937E,
    0x6A826C,
    0xFF029D,
    0xFE8900,
    0x7A4782,
    0x7E2DD2,
    0x85A900,
    0xFF0056,
    0xA42400,
    0x00AE7E,
    0x683D3B,
    0xBDC6FF,
    0x263400,
    0xBDD393,
    0x00B917,
    0x9E008E,
    0x001544,
    0xC28C9F,
    0xFF74A3,
    0x01D0FF,
    0x004754,
    0xE56FFE,
    0x788231,
    0x0E4CA1,
    0x91D0CB,
    0xBE9970,
    0x968AE8,
    0xBB8800,
    0x43002C,
    0xDEFF74,
    0x00FFC6,
    0xFFE502,
    0x620E00,
    0x008F9C,
    0x98FF52,
    0x7544B1,
    0xB500FF,
    0x00FF78,
    0xFF6E41,
    0x005F39,
    0x6B6882,
    0x5FAD4E,
    0xA75740,
    0xA5FFD2,
    0xFFB167,
    0x009BFF,
    0xE85EBE,
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

Point SpriteRenderer::CheckWhichHexagon(int _x, int _y, float a)
{
    float x = _x - a;
    float y = _y - 0.866*a;
    float x2 = x / a;
    float y2 = y / a;
    int q = round(2./3 * x2);
    int r = round(-1./3 * x2  +  sqrt(3)/3 * y2);
    Point p = fromAxial(q,r);
    return p;
}


void SpriteRenderer::DrawHexagon(const Hexagon &hex,float size,glm::vec3 color)
{
    size *= resizeMultiplier;
    float smallSize = size * 0.8;
    color = glm::vec3(1.0f,1.0f,1.0f);
    if (hex.getOwnerId()!=0) {
        color = palette[hex.getOwnerId()%10];
    }
    if (hex.getResident()!=Resident::Water) {
        if (hex.getX()%2==0) {
            this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex.getX()*size * 3/4 + displacementX, hex.getY()*size*sqrt(3)/2+ displacementY) , glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
            if (hex.getResident()==Resident::Warrior1)
            {
                this->DrawSprite(ResourceManager::GetTexture("s1"),glm::vec2(hex.getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4 - smallSize*sqrt(3)/4 + displacementY), glm::vec2(size*0.8,size*0.8), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
            else if (hex.getResident()==Resident::Castle)
            {
                this->DrawSprite(ResourceManager::GetTexture("lw"),glm::vec2(hex.getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4 - smallSize*sqrt(3)/4 + displacementY), glm::vec2(size*0.6,size*0.6), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
        }
        else {
            this->DrawSprite(ResourceManager::GetTexture("hexagon"), glm::vec2(hex.getX()*size * 3/4 + displacementX, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + displacementY), glm::vec2(size,size*sqrt(3)/2), 0.0f, color);
            if (hex.getResident()==Resident::Warrior1)
            {
                this->DrawSprite(ResourceManager::GetTexture("s1"),glm::vec2(hex.getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + size*sqrt(3)/4 - smallSize*sqrt(3)/4 + displacementY), glm::vec2(size*0.8,size*0.8), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
            else if (hex.getResident()==Resident::Castle)
            {
                this->DrawSprite(ResourceManager::GetTexture("lw"),glm::vec2(hex.getX()*size * 3/4 + displacementX + size/2 - smallSize/2, hex.getY()*size*sqrt(3)/2 + size*sqrt(3)/4 + size*sqrt(3)/4 - smallSize*sqrt(3)/4 + displacementY), glm::vec2(size*0.6,size*0.6), 0.0f,glm::vec3(1.0f,1.0f,1.0f));
            }
        }
    }
}



void SpriteRenderer::DrawBoard(Board *board, int width, int height)
{
    // std::cout << "Width: " << width << " " << "Height: " << height << std::endl;
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
    this -> InitPalette();
    glBindVertexArray(this->quadVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}