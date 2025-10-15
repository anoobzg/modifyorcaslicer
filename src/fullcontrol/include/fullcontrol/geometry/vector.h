/**
 * @file vector.h
 * @brief 向量计算函数
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_VECTOR_H
#define FULLCONTROL_VECTOR_H

#include "fullcontrol/core/point.h"
#include <cmath>

namespace fullcontrol {

    /**
     * @brief 计算两点之间的向量
     * @param from 起始点
     * @param to 终点
     * @return 向量点
     */
    Point vectorBetween(const Point& from, const Point& to);

    /**
     * @brief 计算向量的长度
     * @param vector 向量点
     * @return 向量长度
     */
    double vectorLength(const Point& vector);

    /**
     * @brief 计算向量的单位向量
     * @param vector 向量点
     * @return 单位向量
     */
    Point unitVector(const Point& vector);

    /**
     * @brief 计算两个向量的点积
     * @param v1 第一个向量
     * @param v2 第二个向量
     * @return 点积值
     */
    double dotProduct(const Point& v1, const Point& v2);

    /**
     * @brief 计算两个向量的叉积
     * @param v1 第一个向量
     * @param v2 第二个向量
     * @return 叉积向量
     */
    Point crossProduct(const Point& v1, const Point& v2);

    /**
     * @brief 计算两个向量之间的角度
     * @param v1 第一个向量
     * @param v2 第二个向量
     * @return 角度（弧度）
     */
    double angleBetween(const Point& v1, const Point& v2);

} // namespace fullcontrol

#endif // FULLCONTROL_VECTOR_H
