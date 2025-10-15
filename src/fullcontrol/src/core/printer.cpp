/**
 * @file printer.cpp
 * @brief 3D打印机类实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/core/printer.h"
#include <sstream>
#include <iomanip>

namespace fullcontrol {

    Printer::Printer(std::optional<double> print_speed_val, 
                     std::optional<double> travel_speed_val)
        : print_speed(print_speed_val), travel_speed(travel_speed_val) {
    }

    Printer::Printer(double print_speed_val, double travel_speed_val)
        : print_speed(print_speed_val), travel_speed(travel_speed_val) {
    }

    void Printer::setPrintSpeed(double speed) {
        print_speed = speed;
    }

    void Printer::setTravelSpeed(double speed) {
        travel_speed = speed;
    }

    double Printer::getPrintSpeed() const {
        return print_speed.value_or(60.0); // 默认60单位/分钟
    }

    double Printer::getTravelSpeed() const {
        return travel_speed.value_or(120.0); // 默认120单位/分钟
    }

    bool Printer::hasPrintSpeed() const {
        return print_speed.has_value();
    }

    bool Printer::hasTravelSpeed() const {
        return travel_speed.has_value();
    }

    std::string Printer::toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "Printer(";
        
        if (print_speed.has_value()) {
            oss << "print_speed=" << print_speed.value();
        } else {
            oss << "print_speed=None";
        }
        
        oss << ", ";
        
        if (travel_speed.has_value()) {
            oss << "travel_speed=" << travel_speed.value();
        } else {
            oss << "travel_speed=None";
        }
        
        oss << ")";
        return oss.str();
    }

    Printer Printer::copy() const {
        return Printer(print_speed, travel_speed);
    }

} // namespace fullcontrol
