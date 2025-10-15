/**
 * @file state.cpp
 * @brief 状态管理类实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/core/state.h"
#include "fullcontrol/gcode/gcode_generator.h"
#include <sstream>
#include <iomanip>

namespace fullcontrol {

    State::State(const std::vector<std::shared_ptr<void>>& steps_list, 
                 const Printer& printer_config)
        : steps(steps_list), printer(printer_config), controls(nullptr) {
        // 初始化当前位置为原点
        current_position = Point(0.0, 0.0, 0.0);
    }

    State::State(const std::vector<std::shared_ptr<void>>& steps_list, 
                 const GcodeControls& gcode_controls)
        : steps(steps_list), controls(std::make_unique<GcodeControls>(gcode_controls)) {
        // 初始化当前位置为原点
        current_position = Point(0.0, 0.0, 0.0);
    }

    void State::moveTo(const Point& point, bool is_travel) {
        std::ostringstream gcode_line;
        gcode_line << std::fixed << std::setprecision(3);
        
        if (is_travel) {
            gcode_line << "G0 "; // 快速移动
        } else {
            gcode_line << "G1 "; // 线性移动
        }
        
        if (point.hasX()) {
            gcode_line << "X" << point.getX() << " ";
        }
        if (point.hasY()) {
            gcode_line << "Y" << point.getY() << " ";
        }
        if (point.hasZ()) {
            gcode_line << "Z" << point.getZ() << " ";
        }
        
        // 设置速度
        if (is_travel) {
            gcode_line << "F" << printer.getTravelSpeed() * 60; // 转换为mm/min
        } else {
            gcode_line << "F" << printer.getPrintSpeed() * 60; // 转换为mm/min
        }
        
        gcode.push_back(gcode_line.str());
        current_position = point;
    }

    void State::setExtruding(bool extruding) {
        if (extruding) {
            gcode.push_back("M104 S200 ; Turn on extruder");
        } else {
            gcode.push_back("M104 S0 ; Turn off extruder");
        }
    }

    void State::setSpeed(double speed) {
        std::ostringstream gcode_line;
        gcode_line << "F" << speed * 60; // 转换为mm/min
        gcode.push_back(gcode_line.str());
    }

    void State::addGcodeLine(const std::string& gcode_line) {
        gcode.push_back(gcode_line);
    }

} // namespace fullcontrol
