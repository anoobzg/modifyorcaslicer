#ifndef _slic3r_GCodePanel_hpp_
#define _slic3r_GCodePanel_hpp_
#include <memory>
#include <functional>
#include "libslic3r/GCode/GCodeProcessor.hpp"

namespace Slic3r {
//class GCodeProcessorResult;

namespace GUI {
class IMSlider;

namespace GCode {
class GCodeViewerData;
class GCodeViewInstance;

class GCodePanel
{
public:
    GCodePanel();
    ~GCodePanel();

    void set_refresh_func(std::function<void(bool)>& func);
    void set_select_part_func(std::function<void(int)>& func);
    void set_select_mode_func(std::function<void(int)>& func);
    void set_schedule_background_process(std::function<void()>& func);

    void set_instance(std::shared_ptr<GCodeViewInstance> instance);
    void render(float &legend_height, int canvas_width, int canvas_height, int right_margin);
    void render_all_plates_stats(const std::vector<const GCodeProcessorResult*>& gcode_result_list, bool show /*= true*/);
    void render_gcode_window(float top, float bottom, float right) const;
    void render_group_window(float top, float bottom, float right, int count);

    IMSlider* get_moves_slider();
    IMSlider* get_layers_slider();
    void check_layers_slider_values(std::vector<CustomGCode::Item>& ticks_from_model, const std::vector<double>& layers_z);
    bool switch_one_layer_mode();
    void _update_layers_slider_mode();
    void update_layers_slider(const std::vector<double>& layers_z, bool keep_z_range);

    void enable_moves_slider(bool enable) const;
    void update_moves_slider(bool set_to_max = false);
    void update_layers_slider_mode();

private:
    void render_legend(float &legend_height, int canvas_width, int canvas_height, int right_margin);
    void push_combo_style();
    void pop_combo_style();
    void render_sliders(int canvas_width, int canvas_height);

    IMSlider* m_moves_slider;
    IMSlider* m_layers_slider;
    std::shared_ptr<GCodeViewerData> m_data = nullptr;
    
    bool m_legend_enabled{ true };
    float m_scale = 1.0;

    std::function<void(bool)> m_refresh_func;
    std::function<void(int)> m_select_part_func;
    std::function<void(int)> m_select_mode_func;
    std::function<void()> m_schedule_background_process;
};

};
};
};

#endif