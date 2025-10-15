/**
 * @file arcs.h
 * @brief 弧线生成函数
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_ARCS_H
#define FULLCONTROL_ARCS_H

#include "fullcontrol/core/point.h"
#include <vector>
#include <cmath>

namespace fullcontrol {

    /**
     * @brief 生成2D XY弧线
     * @param centre 弧线中心
     * @param radius 半径
     * @param start_angle 起始角度（弧度）
     * @param arc_angle 弧线角度（弧度）
     * @param segments 分段数
     * @return 表示弧线的点列表
     */
    std::vector<Point> arcXY(const Point& centre, 
                             double radius, 
                             double start_angle, 
                             double arc_angle, 
                             int segments);

    /**
     * @brief 生成可变半径的2D XY弧线
     * @param centre 弧线中心
     * @param start_radius 起始半径
     * @param start_angle 起始角度（弧度）
     * @param arc_angle 弧线角度（弧度）
     * @param segments 分段数
     * @param radius_change 半径变化量
     * @param z_change Z轴变化量
     * @return 表示可变半径弧线的点列表
     */
    std::vector<Point> variable_arcXY(const Point& centre, 
                                      double start_radius, 
                                      double start_angle, 
                                      double arc_angle, 
                                      int segments, 
                                      double radius_change, 
                                      double z_change);

    /**
     * @brief 生成2D XY椭圆弧线
     * @param centre 椭圆中心
     * @param a 椭圆宽度（半长轴）
     * @param b 椭圆高度（半短轴）
     * @param start_angle 起始角度（弧度）
     * @param arc_angle 弧线角度（弧度）
     * @param segments 分段数
     * @return 表示椭圆弧线的点列表
     */
    std::vector<Point> elliptical_arcXY(const Point& centre, 
                                        double a, 
                                        double b, 
                                        double start_angle, 
                                        double arc_angle, 
                                        int segments);

} // namespace fullcontrol

#endif // FULLCONTROL_ARCS_H
