#include<string>
class Color{
public:
    Color(){}

    Color(std::string name){
        if(name=="Black"){RGB[0]=0.0f,RGB[1]=0.0f,RGB[2]=0.0f,RGB[3]=1.0f;}
        else if(name=="White"){RGB[0]=1.0f,RGB[1]=1.0f,RGB[2]=1.0f,RGB[3]=1.0f;}
        else if(name=="Red"){RGB[0]=1.0f,RGB[1]=0.0f,RGB[2]=0.0f,RGB[3]=1.0f;}
        else if(name=="Green"){RGB[0]=0.0f,RGB[1]=1.0f,RGB[2]=0.0f,RGB[3]=1.0f;}
        else if(name=="Blue"){RGB[0]=0.0f,RGB[1]=0.0f,RGB[2]=1.0f,RGB[3]=1.0f;}
        else if(name=="Yellow"){RGB[0]=1.0f,RGB[1]=1.0f,RGB[2]=0.0f,RGB[3]=1.0f;}
        else if(name=="Cyan"){RGB[0]=0.0f,RGB[1]=1.0f,RGB[2]=1.0f,RGB[3]=1.0f;}
        else if(name=="Pink"){RGB[0]=1.0f,RGB[1]=0.0f,RGB[2]=1.0f,RGB[3]=1.0f;}
    }

    float RGB[4];
};