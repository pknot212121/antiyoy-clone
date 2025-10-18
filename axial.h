#pragma once

class Axial{
    public:
    Axial(int _q,int _r): q(_q), r(_r) {} 
    friend bool operator<(const Axial& ax1, const Axial& ax2){
        if (ax1.q < ax2.q) return true;   
        if (ax2.q < ax1.q) return false;  
        return ax1.r < ax2.r;
    }
    int q;
    int r;
};