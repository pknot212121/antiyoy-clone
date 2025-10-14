#include "Hexagon.h"
#include <iostream>

Hexagon::Hexagon(float x, float y, float R){
    float y2 = y-R;
    float sixty = 3.14/2;
    startX = 10.0f;
    startY = y;
    for(int i=0;i<6;i++){
        points.push_back(Point(x,y));
        points.push_back(Point(x*cos(sixty*i)-y2*sin(sixty*i),x*sin(sixty*i)+y2*cos(sixty*i)));
        points.push_back(Point(x*cos(sixty*(i+1))-y2*sin(sixty*(i+1)),x*sin(sixty*(i+1))+y2*cos(sixty*(i+1))));
        std::cout << points.size();
    }
}