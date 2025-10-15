/**
 * @file fullcontrol.h
 * @brief FullControl C++ 主头文件
 * @author FullControl C++ Team
 * @version 1.0.0
 * 
 * FullControl C++ 是一个用于3D打印机路径控制的C++库，
 * 允许用户完全控制打印路径和生成G代码。
 */

#ifndef FULLCONTROL_H
#define FULLCONTROL_H

// 核心组件
#include "core/point.h"
#include "core/printer.h"
#include "core/state.h"

// 几何功能
#include "geometry/shapes.h"
#include "geometry/arcs.h"
#include "geometry/vector.h"

// G代码生成
#include "gcode/gcode_generator.h"
#include "gcode/commands.h"

// 最大体积流量计算
#include "max_volumetric_flow/max_volumetric_flow.h"

/**
 * @namespace fullcontrol
 * @brief FullControl C++ 库的主命名空间
 */
namespace fullcontrol {

    /**
     * @brief 库版本信息
     */
    constexpr const char* VERSION = "1.0.0";
    
    /**
     * @brief 库描述
     */
    constexpr const char* DESCRIPTION = "FullControl C++ - 3D打印机路径控制库";

} // namespace fullcontrol

#endif // FULLCONTROL_H
