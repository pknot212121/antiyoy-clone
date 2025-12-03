#version 330 core

layout (location = 0) in vec4 vertex;     // <vec2 pos, vec2 tex> (0..1)
layout (location = 1) in vec2 aOffset;    // Instancja: Pozycja (x, y)
layout (location = 2) in vec3 aColor;     // Instancja: Kolor
layout (location = 3) in float aRotation; // Instancja: Obrót w stopniach (NOWE)
layout (location = 4) in vec2 aSize;      // Instancja: Rozmiar (w, h) (NOWE)

out vec2 TexCoords;
out vec3 HexColor;

uniform mat4 projection;
uniform vec2 viewOffset;
uniform float zoom;

void main()
{
    TexCoords = vertex.zw;
    HexColor = aColor;

    // 1. Pobieramy bazową pozycję wierzchołka (zakładamy 0.0 do 1.0 z Twojego initRenderData)
    vec2 pos = vertex.xy;

    // 2. Skalowanie (Twoje glm::scale)
    pos = pos * aSize * zoom;

    // 3. Obsługa obrotu wokół środka (Twoje pivoty)
    // Przesuwamy środek do (0,0), żeby obrócić wokół własnej osi
    pos -= (aSize * 0.5) * zoom;

    // Matematyka obrotu 2D (Rotation Matrix)
    float rad = radians(aRotation);
    float s = sin(rad);
    float c = cos(rad);

    // Mnożenie macierzy obrotu 2x2 przez wektor
    vec2 rotatedPos;
    rotatedPos.x = pos.x * c - pos.y * s;
    rotatedPos.y = pos.x * s + pos.y * c;
    pos = rotatedPos;

    // Przesuwamy z powrotem (cofnięcie pivotu)
    //pos += (aSize * 0.5);

    // 4. Translacja na pozycję świata (Twoje glm::translate(position))
    pos += aOffset * zoom;
    pos += viewOffset;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}