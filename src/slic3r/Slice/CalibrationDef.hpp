#ifndef CALIBRATION_DEF_HPP
#define CALIBRATION_DEF_HPP
namespace Slic3r { namespace GUI {

enum class CalibrationStyle : int
{
    CALI_STYLE_DEFAULT = 0,
    CALI_STYLE_X1,
    CALI_STYLE_P1P,
};

enum CalibrationFilamentMode {
    /* calibration single filament at once */
    CALI_MODEL_SINGLE = 0,
    /* calibration multi filament at once */
    CALI_MODEL_MULITI,
};

enum CalibrationMethod {
    CALI_METHOD_MANUAL = 0,
    CALI_METHOD_AUTO,
    CALI_METHOD_NONE,
};

enum class CaliPageType {
    CALI_PAGE_START = 0,
    CALI_PAGE_PRESET,
    CALI_PAGE_CALI,
    CALI_PAGE_COARSE_SAVE,
    CALI_PAGE_FINE_CALI,
    CALI_PAGE_FINE_SAVE,
    CALI_PAGE_PA_SAVE,
    CALI_PAGE_FLOW_SAVE,
    CALI_PAGE_COMMON_SAVE,
};

}}
#endif CALIBRATION_DEF_HPP