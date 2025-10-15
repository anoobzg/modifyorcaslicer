/**
 * @file point.h
 * @brief 3D点类定义
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_POINT_H
#define FULLCONTROL_POINT_H

#include <optional>
#include <cmath>
#include <string>

namespace fullcontrol {

    /**
     * @class Point
     * @brief 表示3D空间中的一个点
     * 
     * 这个类表示3D笛卡尔坐标系中的一个点，包含x、y、z坐标。
     * 坐标值是可选的，允许部分坐标未定义。
     */
    class Point {
    public:
        std::optional<double> x;  ///< X坐标
        std::optional<double> y;  ///< Y坐标
        std::optional<double> z;  ///< Z坐标

        /**
         * @brief 默认构造函数
         */
        Point() = default;

        /**
         * @brief 构造函数
         * @param x_val X坐标值
         * @param y_val Y坐标值
         * @param z_val Z坐标值
         */
        Point(std::optional<double> x_val, 
              std::optional<double> y_val, 
              std::optional<double> z_val);

        /**
         * @brief 构造函数（所有坐标）
         * @param x_val X坐标值
         * @param y_val Y坐标值
         * @param z_val Z坐标值
         */
        Point(double x_val, double y_val, double z_val);

        /**
         * @brief 复制构造函数
         * @param other 另一个Point对象
         */
        Point(const Point& other) = default;

        /**
         * @brief 移动构造函数
         * @param other 另一个Point对象
         */
        Point(Point&& other) = default;

        /**
         * @brief 赋值操作符
         * @param other 另一个Point对象
         * @return 当前对象的引用
         */
        Point& operator=(const Point& other) = default;

        /**
         * @brief 移动赋值操作符
         * @param other 另一个Point对象
         * @return 当前对象的引用
         */
        Point& operator=(Point&& other) = default;

        /**
         * @brief 析构函数
         */
        ~Point() = default;

        /**
         * @brief 设置坐标值
         * @param x_val X坐标值
         * @param y_val Y坐标值
         * @param z_val Z坐标值
         */
        void set(double x_val, double y_val, double z_val);

        /**
         * @brief 设置X坐标
         * @param x_val X坐标值
         */
        void setX(double x_val);

        /**
         * @brief 设置Y坐标
         * @param y_val Y坐标值
         */
        void setY(double y_val);

        /**
         * @brief 设置Z坐标
         * @param z_val Z坐标值
         */
        void setZ(double z_val);

        /**
         * @brief 获取X坐标
         * @return X坐标值，如果未设置则返回0.0
         */
        double getX() const;

        /**
         * @brief 获取Y坐标
         * @return Y坐标值，如果未设置则返回0.0
         */
        double getY() const;

        /**
         * @brief 获取Z坐标
         * @return Z坐标值，如果未设置则返回0.0
         */
        double getZ() const;

        /**
         * @brief 检查X坐标是否已设置
         * @return 如果X坐标已设置返回true，否则返回false
         */
        bool hasX() const;

        /**
         * @brief 检查Y坐标是否已设置
         * @return 如果Y坐标已设置返回true，否则返回false
         */
        bool hasY() const;

        /**
         * @brief 检查Z坐标是否已设置
         * @return 如果Z坐标已设置返回true，否则返回false
         */
        bool hasZ() const;

        /**
         * @brief 计算到另一个点的距离
         * @param other 另一个点
         * @return 两点之间的距离
         */
        double distanceTo(const Point& other) const;

        /**
         * @brief 计算到另一个点的距离（2D，忽略Z坐标）
         * @param other 另一个点
         * @return 两点之间的距离（2D）
         */
        double distanceTo2D(const Point& other) const;

        /**
         * @brief 点加法
         * @param other 另一个点
         * @return 新的点
         */
        Point operator+(const Point& other) const;

        /**
         * @brief 点减法
         * @param other 另一个点
         * @return 新的点
         */
        Point operator-(const Point& other) const;

        /**
         * @brief 标量乘法
         * @param scalar 标量值
         * @return 新的点
         */
        Point operator*(double scalar) const;

        /**
         * @brief 标量除法
         * @param scalar 标量值
         * @return 新的点
         */
        Point operator/(double scalar) const;

        /**
         * @brief 点相等比较
         * @param other 另一个点
         * @return 如果两点相等返回true，否则返回false
         */
        bool operator==(const Point& other) const;

        /**
         * @brief 点不等比较
         * @param other 另一个点
         * @return 如果两点不等返回true，否则返回false
         */
        bool operator!=(const Point& other) const;

        /**
         * @brief 转换为字符串
         * @return 点的字符串表示
         */
        std::string toString() const;

        /**
         * @brief 创建点的副本
         * @return 新的点对象
         */
        Point copy() const;
    };

    // 全局操作符重载
    Point operator*(double scalar, const Point& point);

} // namespace fullcontrol

#endif // FULLCONTROL_POINT_H
