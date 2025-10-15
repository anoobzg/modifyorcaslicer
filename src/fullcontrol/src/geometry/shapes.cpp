/**
 * @file shapes.cpp
 * @brief 几何形状生成函数实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/geometry/shapes.h"
#include "fullcontrol/geometry/arcs.h"
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cmath>

namespace fullcontrol {

    std::vector<Point> rectangleXY(const Point& start_point, 
                                   double x_size, 
                                   double y_size, 
                                   bool cw) {
        std::vector<Point> points;
        
        // 根据方向计算四个角点
        Point point1(start_point.getX() + x_size * (!cw), 
                     start_point.getY() + y_size * cw, 
                     start_point.getZ());
        Point point2(start_point.getX() + x_size, 
                     start_point.getY() + y_size, 
                     start_point.getZ());
        Point point3(start_point.getX() + x_size * cw, 
                     start_point.getY() + y_size * (!cw), 
                     start_point.getZ());
        
        points.push_back(start_point.copy());
        points.push_back(point1);
        points.push_back(point2);
        points.push_back(point3);
        points.push_back(start_point.copy());
        
        return points;
    }

    std::vector<Point> circleXY(const Point& centre, 
                                double radius, 
                                double start_angle, 
                                int segments, 
                                bool cw) {
        const double tau = 2.0 * M_PI;
        double arc_angle = tau * (1.0 - (2.0 * cw));
        return arcXY(centre, radius, start_angle, arc_angle, segments);
    }

    std::vector<Point> circleXY_3pt(const Point& pt1, 
                                    const Point& pt2, 
                                    const Point& pt3, 
                                    std::optional<double> start_angle,
                                    std::optional<bool> start_at_first_point,
                                    int segments, 
                                    bool cw) {
        Point centre = geometry_utils::centreXY_3pt(pt1, pt2, pt3);
        double radius = pt1.distanceTo2D(centre);
        
        if (start_angle.has_value() && start_at_first_point.has_value()) {
            throw std::invalid_argument("start_angle and start_at_first_point cannot be set at the same time");
        }
        
        double actual_start_angle;
        if (start_angle.has_value()) {
            actual_start_angle = start_angle.value();
        } else {
            if (!start_at_first_point.has_value()) {
                throw std::invalid_argument("neither start_angle or start_at_first_point set for circleXY_3pt()");
            }
            actual_start_angle = geometry_utils::angleToPoint(centre, pt1);
        }
        
        const double tau = 2.0 * M_PI;
        double arc_angle = tau * (1.0 - (2.0 * cw));
        return arcXY(centre, radius, actual_start_angle, arc_angle, segments);
    }

    std::vector<Point> ellipseXY(const Point& centre, 
                                 double a, 
                                 double b, 
                                 double start_angle, 
                                 int segments, 
                                 bool cw) {
        const double tau = 2.0 * M_PI;
        double arc_angle = tau * (1.0 - (2.0 * cw));
        return elliptical_arcXY(centre, a, b, start_angle, arc_angle, segments);
    }

    std::vector<Point> polygonXY(const Point& centre, 
                                 double enclosing_radius, 
                                 double start_angle, 
                                 int sides, 
                                 bool cw) {
        const double tau = 2.0 * M_PI;
        double arc_angle = tau * (1.0 - (2.0 * cw));
        return arcXY(centre, enclosing_radius, start_angle, arc_angle, sides);
    }

    std::vector<Point> spiralXY(const Point& centre, 
                                double start_radius, 
                                double end_radius, 
                                double start_angle, 
                                double n_turns, 
                                int segments, 
                                bool cw) {
        const double tau = 2.0 * M_PI;
        double arc_angle = n_turns * tau * (1.0 - (2.0 * cw));
        double radius_change = end_radius - start_radius;
        return variable_arcXY(centre, start_radius, start_angle, arc_angle, segments, radius_change, 0.0);
    }

    std::vector<Point> helixZ(const Point& centre, 
                              double start_radius, 
                              double end_radius, 
                              double start_angle, 
                              double n_turns, 
                              double pitch_z, 
                              int segments, 
                              bool cw) {
        const double tau = 2.0 * M_PI;
        double arc_angle = n_turns * tau * (1.0 - (2.0 * cw));
        double radius_change = end_radius - start_radius;
        double z_change = pitch_z * n_turns;
        return variable_arcXY(centre, start_radius, start_angle, arc_angle, segments, radius_change, z_change);
    }

    namespace geometry_utils {
        Point centreXY_3pt(const Point& pt1, const Point& pt2, const Point& pt3) {
            double x1 = pt1.getX(), y1 = pt1.getY();
            double x2 = pt2.getX(), y2 = pt2.getY();
            double x3 = pt3.getX(), y3 = pt3.getY();
            
            // 计算圆心坐标
            double A = x2 - x1;
            double B = y2 - y1;
            double C = x3 - x1;
            double D = y3 - y1;
            
            double E = A * (x1 + x2) + B * (y1 + y2);
            double F = C * (x1 + x3) + D * (y1 + y3);
            
            double G = 2.0 * (A * (y3 - y2) - B * (x3 - x2));
            
            if (std::abs(G) < 1e-9) {
                throw std::invalid_argument("Three points are collinear, cannot define a unique circle");
            }
            
            double cx = (D * E - B * F) / G;
            double cy = (A * F - C * E) / G;
            
            return Point(cx, cy, pt1.getZ());
        }

        double angleToPoint(const Point& centre, const Point& point) {
            double dx = point.getX() - centre.getX();
            double dy = point.getY() - centre.getY();
            return std::atan2(dy, dx);
        }

        double normalizeAngle(double angle) {
            const double tau = 2.0 * M_PI;
            while (angle < 0) angle += tau;
            while (angle >= tau) angle -= tau;
            return angle;
        }
    }

} // namespace fullcontrol
