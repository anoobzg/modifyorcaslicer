/**
 * @file multi_nozzle_calibration.h
 * @brief 多喷嘴校准GCode生成类
 * @author FullControl C++ Team
 * @version 1.0.0
 * 
 * 用于生成多喷嘴校准的GCode文件，将打印床分为4个象限，
 * 每个象限使用不同的喷嘴（T0-T3）打印两个长方形（一个旋转90度）
 */

#ifndef FULLCONTROL_MULTI_NOZZLE_CALIBRATION_H
#define FULLCONTROL_MULTI_NOZZLE_CALIBRATION_H

#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace fullcontrol {

/**
 * @struct Point2D
 * @brief 2D点结构
 */
struct Point2D {
    double x;
    double y;
    
    Point2D() : x(0.0), y(0.0) {}
    Point2D(double x_val, double y_val) : x(x_val), y(y_val) {}
};

/**
 * @struct Quadrant
 * @brief 象限信息
 */
struct Quadrant {
    int tool_index;         ///< 工具索引（0-3）
    std::string tool_name;  ///< 工具名称（T0-T3）
    double center_x;        ///< 象限中心X坐标
    double center_y;        ///< 象限中心Y坐标
    std::string name;       ///< 象限名称（Bottom-Left等）
    
    Quadrant(int idx, const std::string& t_name, double cx, double cy, const std::string& n)
        : tool_index(idx), tool_name(t_name), center_x(cx), center_y(cy), name(n) {}
};

/**
 * @struct MultiNozzleParams
 * @brief 多喷嘴校准参数
 */
struct MultiNozzleParams {
    // 长方形尺寸
    double rect_length;         ///< 长方形长度（mm）
    double rect_width;          ///< 长方形宽度（mm）
    double rect_spacing;        ///< 两个长方形之间的间距（mm）
    
    // 打印床尺寸
    double bed_width;           ///< 打印床宽度（mm）
    double bed_height;          ///< 打印床高度（mm）
    double bed_min_x;           ///< 打印床最小X坐标
    double bed_min_y;           ///< 打印床最小Y坐标
    
    // 打印参数
    double layer_height;        ///< 层高（mm）
    double total_height;        ///< 打印总高度（mm）
    double nozzle_diameter;     ///< 喷嘴直径（mm）
    double filament_diameter;   ///< 耗材直径（mm）
    double print_speed;         ///< 打印速度（mm/s）
    
    // 回抽参数
    double retract_length;      ///< 回抽长度（mm）
    double retract_speed;       ///< 回抽速度（mm/s）
    double z_hop;               ///< 象限间Z抬升高度（mm）
    double z_hop_within_layer;  ///< 层内Z抬升高度（mm）
    
    // 裙边参数
    int skirt_loops;            ///< 裙边圈数
    double skirt_distance;      ///< 裙边距离（mm）
    
    // 温度参数
    int nozzle_temp;            ///< 喷嘴温度（°C）
    int bed_temp;               ///< 热床温度（°C）
    
    // 耗材颜色
    std::vector<std::string> filament_colors;  ///< 耗材颜色列表
    
    // 打印床形状
    std::vector<Point2D> bed_shape;  ///< 打印床形状点列表
    
    MultiNozzleParams()
        : rect_length(10.0), rect_width(2.0), rect_spacing(20.0)
        , bed_width(350.0), bed_height(350.0), bed_min_x(0.0), bed_min_y(0.0)
        , layer_height(0.2), total_height(10.0), nozzle_diameter(0.4), filament_diameter(1.75)
        , print_speed(50.0)
        , retract_length(0.8), retract_speed(30.0), z_hop(10.0), z_hop_within_layer(0.4)
        , skirt_loops(3), skirt_distance(0.2)
        , nozzle_temp(220), bed_temp(55)
    {}
};

/**
 * @class MultiNozzleCalibration
 * @brief 多喷嘴校准GCode生成器
 */
class MultiNozzleCalibration {
public:
    /**
     * @brief 构造函数
     * @param params 校准参数
     */
    explicit MultiNozzleCalibration(const MultiNozzleParams& params);
    
    /**
     * @brief 析构函数
     */
    ~MultiNozzleCalibration() = default;
    
    /**
     * @brief 生成GCode
     * @return GCode字符串
     */
    std::string generateGCode();
    
    /**
     * @brief 设置参数
     * @param params 新的参数
     */
    void setParams(const MultiNozzleParams& params);
    
    /**
     * @brief 获取当前参数
     * @return 当前参数的引用
     */
    const MultiNozzleParams& getParams() const { return m_params; }
    
private:
    MultiNozzleParams m_params;  ///< 校准参数
    
    /**
     * @brief 生成GCode头部
     * @param stream 输出流
     * @param num_layers 总层数
     */
    void generateHeader(std::ostringstream& stream, int num_layers);
    
    /**
     * @brief 生成GCode尾部
     * @param stream 输出流
     */
    void generateFooter(std::ostringstream& stream);
    
    /**
     * @brief 为一个象限生成GCode
     * @param stream 输出流
     * @param quadrant 象限信息
     * @param quad_idx 象限索引
     * @param num_layers 总层数
     * @param e_per_mm 每mm挤出量
     * @param is_last_quadrant 是否是最后一个象限
     */
    void generateQuadrantGCode(std::ostringstream& stream, 
                              const Quadrant& quadrant,
                              size_t quad_idx,
                              int num_layers,
                              double e_per_mm,
                              bool is_last_quadrant = false);
    
    /**
     * @brief 生成一个长方形的裙边
     * @param stream 输出流
     * @param min_x 裙边最小X
     * @param min_y 裙边最小Y
     * @param max_x 裙边最大X
     * @param max_y 裙边最大Y
     * @param e_per_mm 每mm挤出量
     * @param is_first 是否是第一个裙边
     */
    void generateSkirt(std::ostringstream& stream,
                      double min_x, double min_y,
                      double max_x, double max_y,
                      double e_per_mm,
                      bool is_first);
    
    /**
     * @brief 生成一个长方形的轮廓
     * @param stream 输出流
     * @param corners 长方形角点
     * @param e_per_mm 每mm挤出量
     * @param is_first_segment 是否是第一个线段（需要设置速度）
     */
    void generateRectangle(std::ostringstream& stream,
                          const std::vector<Point2D>& corners,
                          double e_per_mm,
                          bool is_first_segment);
    
    /**
     * @brief 计算两点之间的距离
     * @param p1 点1
     * @param p2 点2
     * @return 距离（mm）
     */
    double calculateDistance(const Point2D& p1, const Point2D& p2);
    
    /**
     * @brief 获取象限列表
     * @return 象限列表
     */
    std::vector<Quadrant> getQuadrants();
};

} // namespace fullcontrol

#endif // FULLCONTROL_MULTI_NOZZLE_CALIBRATION_H

