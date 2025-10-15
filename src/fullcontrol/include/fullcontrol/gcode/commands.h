/**
 * @file commands.h
 * @brief G代码命令定义
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_COMMANDS_H
#define FULLCONTROL_COMMANDS_H

#include <string>
#include <optional>

namespace fullcontrol {

    /**
     * @namespace gcode_commands
     * @brief G代码命令常量
     */
    namespace gcode_commands {
        
        // 基本移动命令
        constexpr const char* G0_RAPID_MOVE = "G0";
        constexpr const char* G1_LINEAR_MOVE = "G1";
        constexpr const char* G2_CW_ARC = "G2";
        constexpr const char* G3_CCW_ARC = "G3";
        
        // 单位设置
        constexpr const char* G20_INCHES = "G20";
        constexpr const char* G21_MILLIMETERS = "G21";
        
        // 坐标系统
        constexpr const char* G90_ABSOLUTE = "G90";
        constexpr const char* G91_RELATIVE = "G91";
        
        // 归零
        constexpr const char* G28_HOME = "G28";
        
        // 挤出机命令
        constexpr const char* M82_EXTRUDER_ABSOLUTE = "M82";
        constexpr const char* M83_EXTRUDER_RELATIVE = "M83";
        
        // 温度控制
        constexpr const char* M104_SET_EXTRUDER_TEMP = "M104";
        constexpr const char* M140_SET_BED_TEMP = "M140";
        constexpr const char* M109_WAIT_EXTRUDER_TEMP = "M109";
        constexpr const char* M190_WAIT_BED_TEMP = "M190";
        
        // 风扇控制
        constexpr const char* M106_FAN_ON = "M106";
        constexpr const char* M107_FAN_OFF = "M107";
        
        // 步进电机控制
        constexpr const char* M84_DISABLE_STEPPERS = "M84";
        
        // 注释
        constexpr const char* COMMENT_PREFIX = ";";
        
        /**
         * @brief 生成移动命令
         * @param command G代码命令
         * @param x X坐标（可选）
         * @param y Y坐标（可选）
         * @param z Z坐标（可选）
         * @param e 挤出量（可选）
         * @param f 进给速度（可选）
         * @return G代码字符串
         */
        std::string moveCommand(const char* command, 
                               std::optional<double> x = std::nullopt,
                               std::optional<double> y = std::nullopt,
                               std::optional<double> z = std::nullopt,
                               std::optional<double> e = std::nullopt,
                               std::optional<double> f = std::nullopt);
        
        /**
         * @brief 生成温度设置命令
         * @param command M代码命令
         * @param temperature 温度值
         * @param wait 是否等待温度达到
         * @return G代码字符串
         */
        std::string temperatureCommand(const char* command, 
                                      double temperature, 
                                      bool wait = false);
        
        /**
         * @brief 生成注释
         * @param comment 注释内容
         * @return G代码注释字符串
         */
        std::string comment(const std::string& comment);
        
    } // namespace gcode_commands

} // namespace fullcontrol

#endif // FULLCONTROL_COMMANDS_H
