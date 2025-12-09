#include "resource_manager.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "shaders-arr/instance_vs.h"
#include "shaders-arr/instance_fs.h"
#include "shaders-arr/text_vs.h"
#include "shaders-arr/text_fs.h"
#include "textures-arr/b.h"
#include "textures-arr/castle_256.h"
#include "textures-arr/exclamation.h"
#include "textures-arr/farm1_256.h"
#include "textures-arr/gravestone_256.h"
#include "textures-arr/hexagon.h"
#include "textures-arr/palmTree_256.h"
#include "textures-arr/pineTree_256.h"
#include "textures-arr/shield_placeholder.h"
#include "textures-arr/soilder1_256.h"
#include "textures-arr/soldier2_256.h"
#include "textures-arr/soldier3_256.h"
#include "textures-arr/soldier4_256.h"
#include "textures-arr/strongTower_256.h"
#include "textures-arr/tower_256.h"
// Instantiate static variables
std::map<std::string, Texture2D>    ResourceManager::Textures;
std::map<std::string, Shader>       ResourceManager::Shaders;


Shader& ResourceManager::LoadShader(const char *vShaderFile, const char *fShaderFile, const char *gShaderFile, std::string name)
{
    Shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
    return Shaders[name];
}

Shader& ResourceManager::LoadShader(std::string name)
{
    Shaders[name] = loadDefaultShader();
    return Shaders[name];
}

Shader& ResourceManager::LoadShaderText(std::string name)
{
    Shaders[name] = loadTextShader();
    return Shaders[name];
}

Shader& ResourceManager::GetShader(std::string name)
{
    return Shaders[name];
}

Texture2D& ResourceManager::LoadTexture(const char *file, bool alpha, std::string name)
{
    Textures[name] = loadTextureFromFile(file, alpha);
    return Textures[name];
}

Texture2D& ResourceManager::GetTexture(std::string name)
{
    return Textures[name];
}

void ResourceManager::Clear()
{
    // (properly) delete all shaders	
    for (auto iter : Shaders)
        glDeleteProgram(iter.second.ID);
    // (properly) delete all textures
    for (auto iter : Textures)
        glDeleteTextures(1, &iter.second.ID);
}

Shader ResourceManager::loadShaderFromFile(const char *vShaderFile, const char *fShaderFile, const char *gShaderFile)
{
    // 1. retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::string geometryCode;
    try
    {
        // open files
        std::ifstream vertexShaderFile(vShaderFile);
        std::ifstream fragmentShaderFile(fShaderFile);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vertexShaderFile.rdbuf();
        fShaderStream << fragmentShaderFile.rdbuf();
        // close file handlers
        vertexShaderFile.close();
        fragmentShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
        // if geometry shader path is present, also load a geometry shader
        if (gShaderFile != nullptr)
        {
            std::ifstream geometryShaderFile(gShaderFile);
            std::stringstream gShaderStream;
            gShaderStream << geometryShaderFile.rdbuf();
            geometryShaderFile.close();
            geometryCode = gShaderStream.str();
        }
    }
    catch (std::exception e)
    {
        std::cout << "ERROR::SHADER: Failed to read shader files" << std::endl;
    }
    const char *vShaderCode = vertexCode.c_str();
    const char *fShaderCode = fragmentCode.c_str();
    const char *gShaderCode = geometryCode.c_str();
    // 2. now create shader object from source code
    Shader shader;
    shader.Compile(vShaderCode, fShaderCode, gShaderFile != nullptr ? gShaderCode : nullptr);
    return shader;
}

Shader ResourceManager::loadDefaultShader()
{
    Shader shader;
    shader.Compile(reinterpret_cast<const char*>(shaders_instance_vs),reinterpret_cast<const char*>(shaders_instance_fs),nullptr);
    return shader;
}

Shader ResourceManager::loadTextShader()
{
    Shader shader;
    shader.Compile(shaders_text_vs,shaders_text_fs,nullptr);
    return shader;
}


Texture2D ResourceManager::loadTextureFromFile(const char *file, bool alpha)
{
    // create texture object
    Texture2D texture;
    if (alpha)
    {
        texture.Internal_Format = GL_RGBA;
        texture.Image_Format = GL_RGBA;
    }
    // load image
    int width, height, nrChannels;
    unsigned char* data = stbi_load(file, &width, &height, &nrChannels, 0);
    // now generate texture
    texture.Generate(width, height, data);
    // and finally free image data
    stbi_image_free(data);
    return texture;
}
Texture2D loadTextureFromData(unsigned char* tab,unsigned int len)
{
    Texture2D texture;
    texture.Internal_Format = GL_RGBA;
    texture.Image_Format = GL_RGBA;
    int width, height, nrChannels;
    unsigned char* data = stbi_load_from_memory(tab, len,&width, &height, &nrChannels, 0);
    texture.Generate(width, height, data);
    stbi_image_free(data);
    return texture;
}

void ResourceManager::loadStaticTextures()
{
    Textures["soilder1"] = loadTextureFromData(textures_soilder1_256_png,textures_soilder1_256_png_len);
    Textures["soilder2"] = loadTextureFromData(textures_soldier2_256_png,textures_soldier2_256_png_len);
    Textures["soilder3"] = loadTextureFromData(textures_soldier3_256_png,textures_soldier3_256_png_len);
    Textures["hexagon"] = loadTextureFromData(textures_hexagon_png,textures_hexagon_png_len);
    Textures["soilder4"] = loadTextureFromData(textures_soldier4_256_png,textures_soldier4_256_png_len);
    Textures["exclamation"] = loadTextureFromData(textures_exclamation_png,textures_exclamation_png_len);
    Textures["castle"] = loadTextureFromData(textures_castle_256_png,textures_castle_256_png_len);
    Textures["pine"] = loadTextureFromData(textures_pineTree_256_png,textures_pineTree_256_png_len);
    Textures["palm"] = loadTextureFromData(textures_palmTree_256_png,textures_palmTree_256_png_len);
    Textures["tower"] = loadTextureFromData(textures_tower_256_png,textures_tower_256_png_len);
    Textures["gravestone"] = loadTextureFromData(textures_gravestone_256_png,textures_gravestone_256_png_len);
    Textures["shield"] = loadTextureFromData(textures_shield_placeholder_png,textures_shield_placeholder_png_len);
    Textures["border_placeholder"] = loadTextureFromData(textures_b_png,textures_b_png_len);
    Textures["farm1"] = loadTextureFromData(textures_farm1_256_png,textures_farm1_256_png_len);
    Textures["strongTower"] = loadTextureFromData(textures_strongTower_256_png,textures_strongTower_256_png_len);
}
