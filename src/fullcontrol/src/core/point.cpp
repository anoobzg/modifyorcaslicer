/**
 * @file point.cpp
 * @brief 3D点类实现
 * @author FullControl C++ Team
 * @version 1.0.0
 */

#include "fullcontrol/core/point.h"
#include <sstream>
#include <iomanip>

namespace fullcontrol {

    Point::Point(std::optional<double> x_val, 
                 std::optional<double> y_val, 
                 std::optional<double> z_val)
        : x(x_val), y(y_val), z(z_val) {
    }

    Point::Point(double x_val, double y_val, double z_val)
        : x(x_val), y(y_val), z(z_val) {
    }

    void Point::set(double x_val, double y_val, double z_val) {
        x = x_val;
        y = y_val;
        z = z_val;
    }

    void Point::setX(double x_val) {
        x = x_val;
    }

    void Point::setY(double y_val) {
        y = y_val;
    }

    void Point::setZ(double z_val) {
        z = z_val;
    }

    double Point::getX() const {
        return x.value_or(0.0);
    }

    double Point::getY() const {
        return y.value_or(0.0);
    }

    double Point::getZ() const {
        return z.value_or(0.0);
    }

    bool Point::hasX() const {
        return x.has_value();
    }

    bool Point::hasY() const {
        return y.has_value();
    }

    bool Point::hasZ() const {
        return z.has_value();
    }

    double Point::distanceTo(const Point& other) const {
        double dx = getX() - other.getX();
        double dy = getY() - other.getY();
        double dz = getZ() - other.getZ();
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double Point::distanceTo2D(const Point& other) const {
        double dx = getX() - other.getX();
        double dy = getY() - other.getY();
        return std::sqrt(dx * dx + dy * dy);
    }

    Point Point::operator+(const Point& other) const {
        return Point(
            x.has_value() && other.x.has_value() ? x.value() + other.x.value() : std::optional<double>(),
            y.has_value() && other.y.has_value() ? y.value() + other.y.value() : std::optional<double>(),
            z.has_value() && other.z.has_value() ? z.value() + other.z.value() : std::optional<double>()
        );
    }

    Point Point::operator-(const Point& other) const {
        return Point(
            x.has_value() && other.x.has_value() ? x.value() - other.x.value() : std::optional<double>(),
            y.has_value() && other.y.has_value() ? y.value() - other.y.value() : std::optional<double>(),
            z.has_value() && other.z.has_value() ? z.value() - other.z.value() : std::optional<double>()
        );
    }

    Point Point::operator*(double scalar) const {
        return Point(
            x.has_value() ? x.value() * scalar : std::optional<double>(),
            y.has_value() ? y.value() * scalar : std::optional<double>(),
            z.has_value() ? z.value() * scalar : std::optional<double>()
        );
    }

    Point Point::operator/(double scalar) const {
        if (std::abs(scalar) < 1e-9) {
            throw std::invalid_argument("Division by zero");
        }
        return Point(
            x.has_value() ? x.value() / scalar : std::optional<double>(),
            y.has_value() ? y.value() / scalar : std::optional<double>(),
            z.has_value() ? z.value() / scalar : std::optional<double>()
        );
    }

    bool Point::operator==(const Point& other) const {
        const double epsilon = 1e-9;
        bool x_equal = (!x.has_value() && !other.x.has_value()) || 
                      (x.has_value() && other.x.has_value() && std::abs(x.value() - other.x.value()) < epsilon);
        bool y_equal = (!y.has_value() && !other.y.has_value()) || 
                      (y.has_value() && other.y.has_value() && std::abs(y.value() - other.y.value()) < epsilon);
        bool z_equal = (!z.has_value() && !other.z.has_value()) || 
                      (z.has_value() && other.z.has_value() && std::abs(z.value() - other.z.value()) < epsilon);
        return x_equal && y_equal && z_equal;
    }

    bool Point::operator!=(const Point& other) const {
        return !(*this == other);
    }

    std::string Point::toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "Point(";
        
        if (x.has_value()) {
            oss << "x=" << x.value();
        } else {
            oss << "x=None";
        }
        
        oss << ", ";
        
        if (y.has_value()) {
            oss << "y=" << y.value();
        } else {
            oss << "y=None";
        }
        
        oss << ", ";
        
        if (z.has_value()) {
            oss << "z=" << z.value();
        } else {
            oss << "z=None";
        }
        
        oss << ")";
        return oss.str();
    }

    Point Point::copy() const {
        return Point(x, y, z);
    }

    // 全局操作符重载
    Point operator*(double scalar, const Point& point) {
        return point * scalar;
    }

} // namespace fullcontrol
