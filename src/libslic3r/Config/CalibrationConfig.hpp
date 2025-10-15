#pragma once 
#include <string>

namespace Slic3r {

enum class CalibMode : int {
    Calib_None = 0,
    Calib_PA_Line,
    Calib_PA_Pattern,
    Calib_PA_Tower,
    Calib_Flow_Rate,
    Calib_Temp_Tower,
    Calib_Vol_speed_Tower,
    Calib_VFA_Tower,
    Calib_Retraction_tower,
    Calib_Max_Volumetric_Flow_Tower,
    Calib_Multi_Nozzle
};

enum class CalibState { Start = 0, Preset, Calibration, CoarseSave, FineCalibration, Save, Finish };

struct Calib_Params
{
    Calib_Params() : mode(CalibMode::Calib_None){};
    double    start, end, step;
    bool      print_numbers;

    std::vector<double> accelerations;
    std::vector<double> speeds;

    CalibMode mode;
};

enum FlowRatioCalibrationType {
    COMPLETE_CALIBRATION = 0,
    FINE_CALIBRATION,
};

class X1CCalibInfos
{
public:
    struct X1CCalibInfo
    {
        int         tray_id;
        int         bed_temp;
        int         nozzle_temp;
        float       nozzle_diameter;
        std::string filament_id;
        std::string setting_id;
        float       max_volumetric_speed;
        float       flow_rate = 0.98f; // for flow ratio
    };

    std::vector<X1CCalibInfo> calib_datas;
};

class CaliPresetInfo
{
public:
    int         tray_id;
    float       nozzle_diameter;
    std::string filament_id;
    std::string setting_id;
    std::string name;

    CaliPresetInfo &operator=(const CaliPresetInfo &other)
    {
        this->tray_id         = other.tray_id;
        this->nozzle_diameter = other.nozzle_diameter;
        this->filament_id     = other.filament_id;
        this->setting_id      = other.setting_id;
        this->name            = other.name;
        return *this;
    }
};

struct PrinterCaliInfo
{
    std::string                 dev_id;
    bool                        cali_finished = true;
    float                       cache_flow_ratio;
    std::vector<CaliPresetInfo> selected_presets;
    FlowRatioCalibrationType    cache_flow_rate_calibration_type = FlowRatioCalibrationType::COMPLETE_CALIBRATION;
};

class PACalibResult
{
public:
    enum CalibResult {
        CALI_RESULT_SUCCESS = 0,
        CALI_RESULT_PROBLEM = 1,
        CALI_RESULT_FAILED  = 2,
    };
    int         tray_id;
    int         cali_idx = -1;
    float       nozzle_diameter;
    std::string filament_id;
    std::string setting_id;
    std::string name;
    float       k_value    = 0.0;
    float       n_coef     = 0.0;
    int         confidence = -1; // 0: success  1: uncertain  2: failed
};

struct PACalibIndexInfo
{
    int         tray_id;
    int         cali_idx;
    float       nozzle_diameter;
    std::string filament_id;
};

class FlowRatioCalibResult
{
public:
    int         tray_id;
    float       nozzle_diameter;
    std::string filament_id;
    std::string setting_id;
    float       flow_ratio;
    int         confidence; // 0: success  1: uncertain  2: failed
};

struct DrawBoxOptArgs
{
    DrawBoxOptArgs(int num_perimeters, double height, double line_width, double speed)
        : num_perimeters{num_perimeters}, height{height}, line_width{line_width}, speed{speed} {};
    DrawBoxOptArgs() = default;

    bool   is_filled{false};
    int    num_perimeters;
    double height;
    double line_width;
    double speed;
};

}