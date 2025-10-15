/**
 * @file gcode_generator.h
 * @brief G代码生成器类定义
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#ifndef FULLCONTROL_GCODE_GENERATOR_H
#define FULLCONTROL_GCODE_GENERATOR_H

#include "fullcontrol/core/point.h"
#include "fullcontrol/core/printer.h"
#include "fullcontrol/core/state.h"
#include <vector>
#include <string>
#include <memory>

namespace fullcontrol {

    /**
     * @class GcodeControls
     * @brief G代码生成控制参数
     */
    class GcodeControls {
    public:
        std::string save_as;           ///< 保存文件名（可选）
        bool include_date = false;     ///< 是否在文件名中包含日期
        std::string initial_gcode;     ///< 初始G代码
        std::string final_gcode;       ///< 结束G代码
        double layer_height = 0.2;     ///< 层高
        double extrusion_width = 0.4;  ///< 挤出宽度
        double filament_diameter = 1.75; ///< 耗材直径

        /**
         * @brief 默认构造函数
         */
        GcodeControls() = default;

        /**
         * @brief 初始化控制参数
         */
        void initialize();
    };


    /**
     * @class GcodeGenerator
     * @brief G代码生成器主类
     */
    class GcodeGenerator {
    public:
        /**
         * @brief 从步骤列表生成G代码
         * @param steps 步骤列表
         * @param controls G代码控制参数
         * @param show_tips 是否显示提示
         * @return 生成的G代码字符串
         */
        static std::string generateGcode(const std::vector<std::shared_ptr<void>>& steps,
                                        const GcodeControls& controls,
                                        bool show_tips = false);

        /**
         * @brief 保存G代码到文件
         * @param gcode G代码内容
         * @param filename 文件名
         */
        static void saveGcode(const std::string& gcode, const std::string& filename);

    private:
        /**
         * @brief 处理单个步骤
         * @param state 当前状态
         * @param step 步骤对象
         * @return 生成的G代码行（如果有）
         */
        static std::string processStep(State& state, const std::shared_ptr<void>& step);
        
    };

} // namespace fullcontrol

#endif // FULLCONTROL_GCODE_GENERATOR_H
