/**
 * @file shapes.h
 * @brief 几何形状生成函数
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_SHAPES_H
#define FULLCONTROL_SHAPES_H

#include "fullcontrol/core/point.h"
#include <vector>
#include <cmath>

namespace fullcontrol {

    /**
     * @brief 生成2D XY矩形
     * @param start_point 起始点
     * @param x_size X轴尺寸
     * @param y_size Y轴尺寸
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示矩形的点列表（5个点，首尾相同）
     */
    std::vector<Point> rectangleXY(const Point& start_point, 
                                   double x_size, 
                                   double y_size, 
                                   bool cw = false);

    /**
     * @brief 生成2D XY圆形
     * @param centre 圆心
     * @param radius 半径
     * @param start_angle 起始角度（弧度）
     * @param segments 分段数（默认100）
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示圆形的点列表
     */
    std::vector<Point> circleXY(const Point& centre, 
                                double radius, 
                                double start_angle, 
                                int segments = 100, 
                                bool cw = false);

    /**
     * @brief 通过三点生成2D XY圆形
     * @param pt1 第一个点
     * @param pt2 第二个点
     * @param pt3 第三个点
     * @param start_angle 起始角度（弧度，可选）
     * @param start_at_first_point 是否从第一个点开始（可选）
     * @param segments 分段数（默认100）
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示圆形的点列表
     * @throws std::invalid_argument 如果三点共线
     */
    std::vector<Point> circleXY_3pt(const Point& pt1, 
                                    const Point& pt2, 
                                    const Point& pt3, 
                                    std::optional<double> start_angle = std::nullopt,
                                    std::optional<bool> start_at_first_point = std::nullopt,
                                    int segments = 100, 
                                    bool cw = false);

    /**
     * @brief 生成2D XY椭圆
     * @param centre 椭圆中心
     * @param a 椭圆宽度（半长轴）
     * @param b 椭圆高度（半短轴）
     * @param start_angle 起始角度（弧度）
     * @param segments 分段数（默认100）
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示椭圆的点列表
     */
    std::vector<Point> ellipseXY(const Point& centre, 
                                 double a, 
                                 double b, 
                                 double start_angle, 
                                 int segments = 100, 
                                 bool cw = false);

    /**
     * @brief 生成2D XY多边形
     * @param centre 多边形中心
     * @param enclosing_radius 外接圆半径
     * @param start_angle 起始角度（弧度）
     * @param sides 边数
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示多边形的点列表
     */
    std::vector<Point> polygonXY(const Point& centre, 
                                 double enclosing_radius, 
                                 double start_angle, 
                                 int sides, 
                                 bool cw = false);

    /**
     * @brief 生成2D XY螺旋
     * @param centre 螺旋中心
     * @param start_radius 起始半径
     * @param end_radius 结束半径
     * @param start_angle 起始角度（弧度）
     * @param n_turns 圈数
     * @param segments 分段数
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示螺旋的点列表
     */
    std::vector<Point> spiralXY(const Point& centre, 
                                double start_radius, 
                                double end_radius, 
                                double start_angle, 
                                double n_turns, 
                                int segments, 
                                bool cw = false);

    /**
     * @brief 生成Z方向螺旋（螺旋线）
     * @param centre 螺旋中心
     * @param start_radius 起始半径
     * @param end_radius 结束半径
     * @param start_angle 起始角度（弧度）
     * @param n_turns 圈数
     * @param pitch_z Z轴螺距（每圈的垂直距离）
     * @param segments 分段数
     * @param cw 是否顺时针方向（默认为false，逆时针）
     * @return 表示螺旋线的点列表
     */
    std::vector<Point> helixZ(const Point& centre, 
                              double start_radius, 
                              double end_radius, 
                              double start_angle, 
                              double n_turns, 
                              double pitch_z, 
                              int segments, 
                              bool cw = false);

    // 辅助函数
    namespace geometry_utils {
        /**
         * @brief 计算三点确定的圆心
         * @param pt1 第一个点
         * @param pt2 第二个点
         * @param pt3 第三个点
         * @return 圆心
         * @throws std::invalid_argument 如果三点共线
         */
        Point centreXY_3pt(const Point& pt1, const Point& pt2, const Point& pt3);

        /**
         * @brief 计算两点之间的角度
         * @param centre 中心点
         * @param point 目标点
         * @return 角度（弧度）
         */
        double angleToPoint(const Point& centre, const Point& point);

        /**
         * @brief 将角度标准化到[0, 2π)范围
         * @param angle 输入角度（弧度）
         * @return 标准化后的角度
         */
        double normalizeAngle(double angle);
    }

} // namespace fullcontrol

#endif // FULLCONTROL_SHAPES_H
