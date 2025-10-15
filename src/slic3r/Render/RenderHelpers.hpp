#pragma once
#include "libslic3r/Point.hpp"
#include "slic3r/Render/3DScene.hpp"
#include "slic3r/GUI/Gizmos/GLGizmosManager.hpp"

namespace Slic3r {
class ModelInstance;
namespace GUI {

class Size
{
    int m_width{ 0 };
    int m_height{ 0 };
    float m_scale_factor{ 1.0f };

public:
    Size() = default;
    Size(int width, int height, float scale_factor = 1.0f) : m_width(width), m_height(height), m_scale_factor(scale_factor) {}

    int get_width() const { return m_width; }
    void set_width(int width) { m_width = width; }

    int get_height() const { return m_height; }
    void set_height(int height) { m_height = height; }

    float get_scale_factor() const { return m_scale_factor; }
    void set_scale_factor(float factor) { m_scale_factor = factor; }
};

class RenderTimerEvent : public wxEvent
{
public:
    RenderTimerEvent(wxEventType type, wxTimer& timer)
        : wxEvent(timer.GetId(), type),
        m_timer(&timer)
    {
        SetEventObject(timer.GetOwner());
    }
    int GetInterval() const { return m_timer->GetInterval(); }
    wxTimer& GetTimer() const { return *m_timer; }

    virtual wxEvent* Clone() const { return new RenderTimerEvent(*this); }
    virtual wxEventCategory GetEventCategory() const  { return wxEVT_CATEGORY_TIMER; }
private:
    wxTimer* m_timer;
};

class  ToolbarHighlighterTimerEvent : public wxEvent
{
public:
    ToolbarHighlighterTimerEvent(wxEventType type, wxTimer& timer)
        : wxEvent(timer.GetId(), type),
        m_timer(&timer)
    {
        SetEventObject(timer.GetOwner());
    }
    int GetInterval() const { return m_timer->GetInterval(); }
    wxTimer& GetTimer() const { return *m_timer; }

    virtual wxEvent* Clone() const { return new ToolbarHighlighterTimerEvent(*this); }
    virtual wxEventCategory GetEventCategory() const { return wxEVT_CATEGORY_TIMER; }
private:
    wxTimer* m_timer;
};


class  GizmoHighlighterTimerEvent : public wxEvent
{
public:
    GizmoHighlighterTimerEvent(wxEventType type, wxTimer& timer)
        : wxEvent(timer.GetId(), type),
        m_timer(&timer)
    {
        SetEventObject(timer.GetOwner());
    }
    int GetInterval() const { return m_timer->GetInterval(); }
    wxTimer& GetTimer() const { return *m_timer; }

    virtual wxEvent* Clone() const { return new GizmoHighlighterTimerEvent(*this); }
    virtual wxEventCategory GetEventCategory() const { return wxEVT_CATEGORY_TIMER; }
private:
    wxTimer* m_timer;
};

class RenderStats
{
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_measuring_start;
    int m_fps_out = -1;
    int m_fps_running = 0;
public:
    void increment_fps_counter() { ++m_fps_running; }
    int get_fps() { return m_fps_out; }
    int get_fps_and_reset_if_needed();

};

struct MouseHelper
{
    struct Drag
    {
        static const Point Invalid_2D_Point;
        static const Vec3d Invalid_3D_Point;
        static const int MoveThresholdPx;

        Point start_position_2D;
        Vec3d start_position_3D;
        int move_volume_idx;
        bool move_requires_threshold;
        Point move_start_threshold_position_2D;

    public:
        Drag();
    };

    bool dragging;
    Vec2d position;
    Vec3d scene_position;
    Drag drag;
    bool ignore_left_up;
    bool ignore_right_up;

    MouseHelper();

    void set_start_position_2D_as_invalid() { drag.start_position_2D = Drag::Invalid_2D_Point; }
    void set_start_position_3D_as_invalid() { drag.start_position_3D = Drag::Invalid_3D_Point; }
    void set_move_start_threshold_position_2D_as_invalid() { drag.move_start_threshold_position_2D = Drag::Invalid_2D_Point; }

    bool is_start_position_2D_defined() const { return (drag.start_position_2D != Drag::Invalid_2D_Point); }
    bool is_start_position_3D_defined() const { return (drag.start_position_3D != Drag::Invalid_3D_Point); }
    bool is_move_start_threshold_position_2D_defined() const { return (drag.move_start_threshold_position_2D != Drag::Invalid_2D_Point); }
    bool is_move_threshold_met(const Point& mouse_pos) const {
        return (std::abs(mouse_pos(0) - drag.move_start_threshold_position_2D(0)) > Drag::MoveThresholdPx)
            || (std::abs(mouse_pos(1) - drag.move_start_threshold_position_2D(1)) > Drag::MoveThresholdPx);
    }
};

class GLCanvas3D;
class LabelsHelper
{
    bool m_enabled{ false };
    bool m_shown{ false };
    GLCanvas3D& m_canvas;

public:
    explicit LabelsHelper(GLCanvas3D& canvas) : m_canvas(canvas) {}
    void enable(bool enable) { m_enabled = enable; }
    void show(bool show) { m_shown = m_enabled ? show : false; }
    bool is_shown() const { return m_shown; }
    void render(const std::vector<const ModelInstance*>& sorted_instances) const;
};

class TooltipHelper
{
    std::string m_text;
    std::chrono::steady_clock::time_point m_start_time;
    // Indicator that the mouse is inside an ImGUI dialog, therefore the tooltip should be suppressed.
    bool m_in_imgui = false;

public:
    bool is_empty() const { return m_text.empty(); }
    void set_text(const std::string& text);
    void render(const Vec2d& mouse_position, GLCanvas3D& canvas);
    // Indicates that the mouse is inside an ImGUI dialog, therefore the tooltip should be suppressed.
    void set_in_imgui(bool b) { m_in_imgui = b; }
    bool is_in_imgui() const { return m_in_imgui; }
};

class SlopeHelper
{
    bool m_enabled{ false };
    GLVolumeCollection& m_volumes;
public:
    SlopeHelper(GLVolumeCollection& volumes) : m_volumes(volumes) {}

    void enable(bool enable) { m_enabled = enable; }
    bool is_enabled() const { return m_enabled; }
    void use(bool use) { m_volumes.set_slope_active(m_enabled ? use : false); }
    bool is_used() const { return m_volumes.is_slope_active(); }
    void globalUse(bool use) { m_volumes.set_slope_GlobalActive(m_enabled ? use : false); }
    bool is_GlobalUsed() const { return m_volumes.is_slope_GlobalActive(); }
    void set_normal_angle(float angle_in_deg) const {
        m_volumes.set_slope_normal_z(-::cos(Geometry::deg2rad(90.0f - angle_in_deg)));
    }
};

class SequentialPrintClearance
{
    //BBS: add the height logic
    GLModel m_height_limit;
    GLModel m_fill;
    GLModel m_perimeter;
    bool m_render_fill{ true };
    bool m_visible{ false };

    std::vector<Pointf3s> m_hull_2d_cache;

public:
    //BBS: add the height logic
    void set_polygons(const Polygons& polygons, const std::vector<std::pair<Polygon, float>>& height_polygons);
    void set_render_fill(bool render_fill) { m_render_fill = render_fill; }
    void set_visible(bool visible) { m_visible = visible; }
    void render();

    friend class GLCanvas3D;
};

class RenderTimer : public wxTimer {
private:
    virtual void Notify() override;
};

class ToolbarHighlighterTimer : public wxTimer {
private:
    virtual void Notify() override;
};

class GizmoHighlighterTimer : public wxTimer {
private:
    virtual void Notify() override;
};

class GLToolbarItem;
struct ToolbarHighlighter
{
    void set_timer_owner(wxEvtHandler* owner, int timerid = wxID_ANY);
    void init(GLToolbarItem* toolbar_item, GLCanvas3D* canvas);
    void blink();
    void invalidate();
    bool                    m_render_arrow{ false };
    GLToolbarItem*          m_toolbar_item{ nullptr };
private:
    GLCanvas3D*             m_canvas{ nullptr };
    int				        m_blink_counter{ 0 };
    ToolbarHighlighterTimer m_timer;
};

struct GizmoHighlighter
{
    void set_timer_owner(wxEvtHandler* owner, int timerid = wxID_ANY);
    void init(GLGizmosManager* manager, GLGizmosManager::EType gizmo, GLCanvas3D* canvas);
    void blink();
    void invalidate();
    bool                    m_render_arrow{ false };
    GLGizmosManager::EType  m_gizmo_type;
private:
    GLGizmosManager*        m_gizmo_manager{ nullptr };
    GLCanvas3D*             m_canvas{ nullptr };
    int				        m_blink_counter{ 0 };
    GizmoHighlighterTimer   m_timer;

};

class WipeTowerInfoHelper 
{
protected:
    Vec2d m_pos = {std::nan(""), std::nan("")};
    double m_rotation = 0.;
    BoundingBoxf m_bb;
    // BBS: add partplate logic
    int m_plate_idx = -1;
    friend class GLCanvas3D;

public:
    inline operator bool() const {
        return !std::isnan(m_pos.x()) && !std::isnan(m_pos.y());
    }

    inline const Vec2d& pos() const { return m_pos; }
    inline double rotation() const { return m_rotation; }
    inline const Vec2d bb_size() const { return m_bb.size(); }

    void apply_wipe_tower() const;
};

}
}