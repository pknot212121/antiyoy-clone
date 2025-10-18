#pragma once
#include "Hexagon.h"

class Warrior
{
    public:
    Warrior(){}
    Warrior(Hexagon _hex): hex(_hex) {}
    Hexagon hex;
};