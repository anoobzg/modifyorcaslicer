/**
 * @file commands.cpp
 * @brief G代码命令实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/gcode/commands.h"
#include <sstream>
#include <iomanip>

namespace fullcontrol {

    namespace gcode_commands {

        std::string moveCommand(const char* command, 
                               std::optional<double> x,
                               std::optional<double> y,
                               std::optional<double> z,
                               std::optional<double> e,
                               std::optional<double> f) {
            std::ostringstream gcode;
            gcode << std::fixed << std::setprecision(3);
            gcode << command;
            
            if (x.has_value()) {
                gcode << " X" << x.value();
            }
            if (y.has_value()) {
                gcode << " Y" << y.value();
            }
            if (z.has_value()) {
                gcode << " Z" << z.value();
            }
            if (e.has_value()) {
                gcode << " E" << e.value();
            }
            if (f.has_value()) {
                gcode << " F" << f.value();
            }
            
            return gcode.str();
        }

        std::string temperatureCommand(const char* command, 
                                      double temperature, 
                                      bool wait) {
            std::ostringstream gcode;
            gcode << std::fixed << std::setprecision(1);
            gcode << command << " S" << temperature;
            
            if (wait) {
                gcode << " ; Wait for temperature";
            }
            
            return gcode.str();
        }

        std::string comment(const std::string& comment_text) {
            return std::string(COMMENT_PREFIX) + " " + comment_text;
        }

    } // namespace gcode_commands

} // namespace fullcontrol
