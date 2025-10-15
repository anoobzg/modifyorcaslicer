#ifndef _slic3r_GCodeRenderer_hpp_
#define _slic3r_GCodeRenderer_hpp_

#include "GCodeDefine.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

namespace Slic3r {
class Print;
class DynamicPrintConfig;
namespace GUI {
namespace GCode {
struct Shells;
class GCodeViewerData;
class GCodeRenderer
{
public:
    enum RenderMode 
    {
        Normal = 0,
        Mirror
    };

    GCodeRenderer();
    ~GCodeRenderer();

    void set_render_mode(RenderMode mode);

    void load_shells(const Print* print, bool initialized, bool force_previewing);
    void update_shells_color_by_extruder(const DynamicPrintConfig *config);
    void set_shell_transparency(float alpha);
    void reset_shell();
    void render_shells(int canvas_width, int canvas_height);

    void set_data(GCodeViewerData* data);
    void refresh_render_paths(bool keep_sequential_current_first, bool keep_sequential_current_last);
    void render_toolpaths();

    void render_marker(int canvas_width, int canvas_height, IdexMode indexMode = IdexMode_Pack);

    void render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const BoundingBoxf3& box);
private:
    Shells* m_shells;
    GCodeViewerData* m_data{ NULL };

    bool m_no_render_path;
    std::array<SequentialRangeCap, 2> m_sequential_range_caps;

    RenderMode m_mode { Normal };

};

};
};
};

#endif