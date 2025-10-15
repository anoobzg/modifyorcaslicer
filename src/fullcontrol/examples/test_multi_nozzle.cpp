/**
 * @file test_multi_nozzle.cpp
 * @brief 多喷嘴校准模块测试程序
 */

#include "fullcontrol/multi_nozzle_calibration/multi_nozzle_calibration.h"
#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::cout << "=== Multi-Nozzle Calibration Test ===" << std::endl;
    
    // 创建参数
    fullcontrol::MultiNozzleParams params;
    params.rect_length = 10.0;
    params.rect_width = 2.0;
    params.rect_spacing = 20.0;
    params.bed_width = 350.0;
    params.bed_height = 350.0;
    params.bed_min_x = 0.0;
    params.bed_min_y = 0.0;
    params.layer_height = 0.2;
    params.total_height = 10.0;
    params.nozzle_diameter = 0.4;
    params.filament_diameter = 1.75;
    params.print_speed = 50.0;
    params.retract_length = 0.8;
    params.retract_speed = 30.0;
    params.z_hop = 10.0;
    params.z_hop_within_layer = 0.4;
    params.skirt_loops = 3;
    params.skirt_distance = 0.2;
    params.nozzle_temp = 220;
    params.bed_temp = 55;
    
    // 添加床形状
    params.bed_shape.emplace_back(0, 0);
    params.bed_shape.emplace_back(350, 0);
    params.bed_shape.emplace_back(350, 350);
    params.bed_shape.emplace_back(0, 350);
    
    // 添加耗材颜色
    params.filament_colors.push_back("#A7A9AA");
    params.filament_colors.push_back("#00C1AE");
    params.filament_colors.push_back("#F4E2C1");
    params.filament_colors.push_back("#ED1C24");
    
    std::cout << "Parameters:" << std::endl;
    std::cout << "  Rectangle: " << params.rect_length << " x " << params.rect_width << " mm" << std::endl;
    std::cout << "  Spacing: " << params.rect_spacing << " mm" << std::endl;
    std::cout << "  Print height: " << params.total_height << " mm" << std::endl;
    std::cout << "  Bed size: " << params.bed_width << " x " << params.bed_height << " mm" << std::endl;
    std::cout << std::endl;
    
    // 创建校准器并生成GCode
    fullcontrol::MultiNozzleCalibration calibrator(params);
    
    std::cout << "Generating GCode..." << std::endl;
    std::string gcode = calibrator.generateGCode();
    
    std::cout << "GCode generated: " << gcode.size() << " bytes" << std::endl;
    
    // 保存到文件
    std::string output_file = "test_multi_nozzle_output.gcode";
    std::ofstream file(output_file);
    if (file.is_open()) {
        file << gcode;
        file.close();
        std::cout << "GCode saved to: " << output_file << std::endl;
    } else {
        std::cerr << "Failed to save GCode file!" << std::endl;
        return 1;
    }
    
    // 验证GCode内容
    std::cout << std::endl << "Validating GCode..." << std::endl;
    
    bool has_header = gcode.find("HEADER_BLOCK_START") != std::string::npos;
    bool has_t0 = gcode.find("T0") != std::string::npos;
    bool has_t1 = gcode.find("T1") != std::string::npos;
    bool has_t2 = gcode.find("T2") != std::string::npos;
    bool has_t3 = gcode.find("T3") != std::string::npos;
    bool has_footer = gcode.find("PRINT_END") != std::string::npos;
    bool has_layer_change = gcode.find("LAYER_CHANGE") != std::string::npos;
    bool has_skirt = gcode.find("Skirt for Rectangle") != std::string::npos;
    
    std::cout << "  Header: " << (has_header ? "✓" : "✗") << std::endl;
    std::cout << "  T0 tool: " << (has_t0 ? "✓" : "✗") << std::endl;
    std::cout << "  T1 tool: " << (has_t1 ? "✓" : "✗") << std::endl;
    std::cout << "  T2 tool: " << (has_t2 ? "✓" : "✗") << std::endl;
    std::cout << "  T3 tool: " << (has_t3 ? "✓" : "✗") << std::endl;
    std::cout << "  Footer: " << (has_footer ? "✓" : "✗") << std::endl;
    std::cout << "  Layer changes: " << (has_layer_change ? "✓" : "✗") << std::endl;
    std::cout << "  Skirts: " << (has_skirt ? "✓" : "✗") << std::endl;
    
    if (has_header && has_t0 && has_t1 && has_t2 && has_t3 && 
        has_footer && has_layer_change && has_skirt) {
        std::cout << std::endl << "✅ All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cerr << std::endl << "❌ Some tests FAILED!" << std::endl;
        return 1;
    }
}

