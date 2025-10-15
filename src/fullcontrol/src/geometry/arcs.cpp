/**
 * @file arcs.cpp
 * @brief 弧线生成函数实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/geometry/arcs.h"
#include <cmath>

namespace fullcontrol {

    std::vector<Point> arcXY(const Point& centre, 
                             double radius, 
                             double start_angle, 
                             double arc_angle, 
                             int segments) {
        std::vector<Point> points;
        
        if (segments <= 0) {
            return points;
        }
        
        double angle_step = arc_angle / segments;
        double current_angle = start_angle;
        
        for (int i = 0; i <= segments; ++i) {
            double x = centre.getX() + radius * std::cos(current_angle);
            double y = centre.getY() + radius * std::sin(current_angle);
            double z = centre.getZ();
            
            points.emplace_back(x, y, z);
            current_angle += angle_step;
        }
        
        return points;
    }

    std::vector<Point> variable_arcXY(const Point& centre, 
                                      double start_radius, 
                                      double start_angle, 
                                      double arc_angle, 
                                      int segments, 
                                      double radius_change, 
                                      double z_change) {
        std::vector<Point> points;
        
        if (segments <= 0) {
            return points;
        }
        
        double angle_step = arc_angle / segments;
        double radius_step = radius_change / segments;
        double z_step = z_change / segments;
        
        double current_angle = start_angle;
        double current_radius = start_radius;
        double current_z = centre.getZ();
        
        for (int i = 0; i <= segments; ++i) {
            double x = centre.getX() + current_radius * std::cos(current_angle);
            double y = centre.getY() + current_radius * std::sin(current_angle);
            
            points.emplace_back(x, y, current_z);
            
            current_angle += angle_step;
            current_radius += radius_step;
            current_z += z_step;
        }
        
        return points;
    }

    std::vector<Point> elliptical_arcXY(const Point& centre, 
                                        double a, 
                                        double b, 
                                        double start_angle, 
                                        double arc_angle, 
                                        int segments) {
        std::vector<Point> points;
        
        if (segments <= 0) {
            return points;
        }
        
        double angle_step = arc_angle / segments;
        double current_angle = start_angle;
        
        for (int i = 0; i <= segments; ++i) {
            double x = centre.getX() + a * std::cos(current_angle);
            double y = centre.getY() + b * std::sin(current_angle);
            double z = centre.getZ();
            
            points.emplace_back(x, y, z);
            current_angle += angle_step;
        }
        
        return points;
    }

} // namespace fullcontrol
