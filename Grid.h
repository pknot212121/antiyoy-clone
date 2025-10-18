#include <vector>
#include "Hexagon.h"

/*
A class containg a vector of Hexagons to concatentate all vertices and indices into one
I am not sure if it is an optiml way
*/

class Grid
{

public:
    Grid() {}    
    Grid(Hexagon hex);

    void SaveSetup();
    void Draw();
    void AddHexagon(int q,int r);
    
    void CheckWhichHexagon(int x, int y, int SCR_WIDTH, int SCR_HEIGHT);
    bool CheckIfHexIsInGrid(int q, int r);
    void SaveHexagonFromCoords(int q, int r);

    std::vector<Hexagon> hexagons;
    std::vector<float> allVertices;
    std::vector<float> allVerticesInside;
    std::vector<unsigned int> allIndices;
    unsigned int VBOs[2], VAOs[2], EBOs[2];
    Shader outlineShader = Shader();
    Shader insideShader = Shader();
    float centerX;
    float centerY;
    float radius;
    Hexagon clickedHex;
    bool clicked;


    
};