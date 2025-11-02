#pragma once
#include "spatialTree.hpp"

// ---------------------------
// Tipos 2D
// ---------------------------
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x+o.x, y+o.y); }
    Vec2 operator*(float s) const { return Vec2(x*s, y*s); }
};

struct Ray2 {
    Vec2 origin, dir;
    Ray2(const Vec2& o, const Vec2& d) : origin(o), dir(d) {}
};

struct RayHit2 {
    float t;
    Vec2 point;
};

struct AABB2 {
    Vec2 min, max;
    AABB2() {}
    AABB2(const Vec2& mi, const Vec2& ma) : min(mi), max(ma) {}
    Vec2 Center() const { return Vec2((min.x+max.x)*0.5f, (min.y+max.y)*0.5f); }
    bool Contains(const Vec2& p) const { return p.x>=min.x && p.x<=max.x && p.y>=min.y && p.y<=max.y; }
    Vec2 Min() const { return min; }
    Vec2 Max() const { return max; }
    
    // Contém outro AABB2 completamente
    bool Contains(const AABB2& other) const {
        return (other.min.x >= min.x && other.max.x <= max.x) &&
               (other.min.y >= min.y && other.max.y <= max.y);
    }

    bool Intersects(const AABB2& b) const {
        return !(b.max.x<min.x || b.min.x>max.x || b.max.y<min.y || b.min.y>max.y);
    }
    bool Intersects(const Ray2& ray, RayHit2& hit, float tMax) const {
        float tmin=0.0f, tmax=tMax;
        for(int i=0;i<2;i++){
            float o=(i==0)?ray.origin.x:ray.origin.y;
            float d=(i==0)?ray.dir.x:ray.dir.y;
            float minVal=(i==0)?min.x:min.y;
            float maxVal=(i==0)?max.x:max.y;
            float invD=1.0f/d;
            float t0=(minVal-o)*invD;
            float t1=(maxVal-o)*invD;
            if(invD<0) std::swap(t0,t1);
            tmin = std::max(tmin,t0);
            tmax = std::min(tmax,t1);
            if(tmax<tmin) return false;
        }
        hit.t=tmin;
        hit.point=ray.origin+ray.dir*tmin;
        return true;
    }
};

// Alias para SpatialTree 2D
using SpatialTree2D = cp_api::SpatialTree<Vec2, AABB2, Ray2, RayHit2, 4>; // 4 filhos por nó