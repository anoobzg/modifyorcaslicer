/**
 * @file vector.cpp
 * @brief 向量计算函数实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/geometry/vector.h"
#include <cmath>

namespace fullcontrol {

    Point vectorBetween(const Point& from, const Point& to) {
        return Point(
            to.getX() - from.getX(),
            to.getY() - from.getY(),
            to.getZ() - from.getZ()
        );
    }

    double vectorLength(const Point& vector) {
        return std::sqrt(
            vector.getX() * vector.getX() +
            vector.getY() * vector.getY() +
            vector.getZ() * vector.getZ()
        );
    }

    Point unitVector(const Point& vector) {
        double length = vectorLength(vector);
        if (length < 1e-9) {
            return Point(0.0, 0.0, 0.0);
        }
        return vector / length;
    }

    double dotProduct(const Point& v1, const Point& v2) {
        return v1.getX() * v2.getX() + 
               v1.getY() * v2.getY() + 
               v1.getZ() * v2.getZ();
    }

    Point crossProduct(const Point& v1, const Point& v2) {
        return Point(
            v1.getY() * v2.getZ() - v1.getZ() * v2.getY(),
            v1.getZ() * v2.getX() - v1.getX() * v2.getZ(),
            v1.getX() * v2.getY() - v1.getY() * v2.getX()
        );
    }

    double angleBetween(const Point& v1, const Point& v2) {
        double dot = dotProduct(v1, v2);
        double len1 = vectorLength(v1);
        double len2 = vectorLength(v2);
        
        if (len1 < 1e-9 || len2 < 1e-9) {
            return 0.0;
        }
        
        double cos_angle = dot / (len1 * len2);
        // 确保cos_angle在[-1, 1]范围内，避免数值误差
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        
        return std::acos(cos_angle);
    }

} // namespace fullcontrol
