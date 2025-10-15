/**
 * @file printer.h
 * @brief 3D打印机类定义
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_PRINTER_H
#define FULLCONTROL_PRINTER_H

#include <optional>
#include <string>

namespace fullcontrol {

    /**
     * @class Printer
     * @brief 表示3D打印机的配置和参数
     * 
     * 这个类包含3D打印机的各种设置参数，如打印速度、移动速度等。
     */
    class Printer {
    public:
        std::optional<double> print_speed;   ///< 打印速度（单位/分钟）
        std::optional<double> travel_speed;  ///< 移动速度（单位/分钟）

        /**
         * @brief 默认构造函数
         */
        Printer() = default;

        /**
         * @brief 构造函数
         * @param print_speed_val 打印速度
         * @param travel_speed_val 移动速度
         */
        Printer(std::optional<double> print_speed_val, 
                std::optional<double> travel_speed_val);

        /**
         * @brief 构造函数（所有参数）
         * @param print_speed_val 打印速度
         * @param travel_speed_val 移动速度
         */
        Printer(double print_speed_val, double travel_speed_val);

        /**
         * @brief 复制构造函数
         * @param other 另一个Printer对象
         */
        Printer(const Printer& other) = default;

        /**
         * @brief 移动构造函数
         * @param other 另一个Printer对象
         */
        Printer(Printer&& other) = default;

        /**
         * @brief 赋值操作符
         * @param other 另一个Printer对象
         * @return 当前对象的引用
         */
        Printer& operator=(const Printer& other) = default;

        /**
         * @brief 移动赋值操作符
         * @param other 另一个Printer对象
         * @return 当前对象的引用
         */
        Printer& operator=(Printer&& other) = default;

        /**
         * @brief 析构函数
         */
        ~Printer() = default;

        /**
         * @brief 设置打印速度
         * @param speed 打印速度值
         */
        void setPrintSpeed(double speed);

        /**
         * @brief 设置移动速度
         * @param speed 移动速度值
         */
        void setTravelSpeed(double speed);

        /**
         * @brief 获取打印速度
         * @return 打印速度值，如果未设置则返回默认值
         */
        double getPrintSpeed() const;

        /**
         * @brief 获取移动速度
         * @return 移动速度值，如果未设置则返回默认值
         */
        double getTravelSpeed() const;

        /**
         * @brief 检查打印速度是否已设置
         * @return 如果打印速度已设置返回true，否则返回false
         */
        bool hasPrintSpeed() const;

        /**
         * @brief 检查移动速度是否已设置
         * @return 如果移动速度已设置返回true，否则返回false
         */
        bool hasTravelSpeed() const;

        /**
         * @brief 转换为字符串
         * @return 打印机配置的字符串表示
         */
        std::string toString() const;

        /**
         * @brief 创建打印机的副本
         * @return 新的Printer对象
         */
        Printer copy() const;
    };

} // namespace fullcontrol

#endif // FULLCONTROL_PRINTER_H
