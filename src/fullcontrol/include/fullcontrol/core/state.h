/**
 * @file state.h
 * @brief 状态管理类定义
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_STATE_H
#define FULLCONTROL_STATE_H

#include "fullcontrol/core/point.h"
#include "fullcontrol/core/printer.h"
#include <vector>
#include <memory>
#include <string>

namespace fullcontrol {

    // 前向声明
    class GcodeControls;

    /**
     * @class State
     * @brief 打印状态管理类
     * 
     * 这个类管理打印过程中的各种状态信息，包括当前位置、打印机配置等。
     */
    class State {
    public:
        std::vector<std::shared_ptr<void>> steps;  ///< 步骤列表
        std::vector<std::string> gcode;            ///< 生成的G代码
        size_t i = 0;                              ///< 当前步骤索引
        Point current_position;                     ///< 当前位置
        Printer printer;                           ///< 打印机配置
        std::unique_ptr<GcodeControls> controls;   ///< G代码控制参数（可选）

        /**
         * @brief 默认构造函数
         */
        State() = default;

        /**
         * @brief 构造函数
         * @param steps_list 步骤列表
         * @param printer_config 打印机配置
         */
        State(const std::vector<std::shared_ptr<void>>& steps_list, 
              const Printer& printer_config);

        /**
         * @brief 构造函数（带G代码控制参数）
         * @param steps_list 步骤列表
         * @param gcode_controls G代码控制参数
         */
        State(const std::vector<std::shared_ptr<void>>& steps_list, 
              const GcodeControls& gcode_controls);

        /**
         * @brief 移动到指定位置
         * @param point 目标位置
         * @param is_travel 是否为移动（非打印）
         */
        void moveTo(const Point& point, bool is_travel = false);

        /**
         * @brief 设置挤出机状态
         * @param extruding 是否挤出
         */
        void setExtruding(bool extruding);

        /**
         * @brief 设置打印速度
         * @param speed 速度值
         */
        void setSpeed(double speed);

        /**
         * @brief 添加G代码行
         * @param gcode_line G代码行
         */
        void addGcodeLine(const std::string& gcode_line);
    };

} // namespace fullcontrol

#endif // FULLCONTROL_STATE_H
