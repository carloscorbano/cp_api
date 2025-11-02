struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator*(float s) const { return Vec3(x*s, y*s, z*s); }
};

struct Ray3 {
    Vec3 origin, dir;
    Ray3(const Vec3& o, const Vec3& d) : origin(o), dir(d) {}
};

struct RayHit3 {
    float t;
    Vec3 point;
};

struct AABB3 {
    Vec3 min, max;
    AABB3() {}
    AABB3(const Vec3& mi, const Vec3& ma) : min(mi), max(ma) {}
    Vec3 Center() const { return Vec3((min.x+max.x)*0.5f, (min.y+max.y)*0.5f, (min.z+max.z)*0.5f); }

    Vec3 Min() const { return min; }
    Vec3 Max() const { return max; }

    bool Contains(const Vec3& p) const {
        return p.x>=min.x && p.x<=max.x &&
               p.y>=min.y && p.y<=max.y &&
               p.z>=min.z && p.z<=max.z;
    }

    bool Contains(const AABB3& other) const {
        return (other.min.x >= min.x && other.max.x <= max.x) &&
               (other.min.y >= min.y && other.max.y <= max.y) &&
               (other.min.z >= min.z && other.max.z <= max.z);
    }

    bool Intersects(const AABB3& b) const {
        return !(b.max.x<min.x || b.min.x>max.x ||
                 b.max.y<min.y || b.min.y>max.y ||
                 b.max.z<min.z || b.min.z>max.z);
    }
    bool Intersects(const Ray3& ray, RayHit3& hit, float tMax) const {
        float tmin=0.0f, tmax=tMax;
        for(int i=0;i<3;i++){
            float o=(i==0)?ray.origin.x:(i==1)?ray.origin.y:ray.origin.z;
            float d=(i==0)?ray.dir.x:(i==1)?ray.dir.y:ray.dir.z;
            float minVal=(i==0)?min.x:(i==1)?min.y:min.z;
            float maxVal=(i==0)?max.x:(i==1)?max.y:max.z;
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

// Alias para SpatialTree 3D
using SpatialTree3D = cp_api::SpatialTree<Vec3, AABB3, Ray3, RayHit3, 8>; // 8 filhos por nó (octree)