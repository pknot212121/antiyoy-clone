#version 330 core

layout (location = 0) in vec4 vertex;
layout (location = 1) in vec2 aOffset;
layout (location = 2) in vec3 aColor;
layout (location = 3) in float aRotation;
layout (location = 4) in vec2 aSize;

out vec2 TexCoords;
out vec3 HexColor;

uniform mat4 projection;

void main()
{
    TexCoords = vertex.zw;
    HexColor = aColor;
    vec2 pos = vertex.xy;
    pos = pos * aSize;
    pos -= (aSize * 0.5);
    float rad = radians(aRotation);
    float s = sin(rad);
    float c = cos(rad);
    vec2 rotatedPos;
    rotatedPos.x = pos.x * c - pos.y * s;
    rotatedPos.y = pos.x * s + pos.y * c;
    pos = rotatedPos + (aSize*0.5);
    pos += aOffset;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}