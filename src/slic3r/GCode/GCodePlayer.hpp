#ifndef _slic3r_GCodePlayer_hpp
#define _slic3r_GCodePlayer_hpp

#include <vector>
#include <chrono>

namespace Slic3r {
namespace GUI {
class IMSlider;
namespace GCode {
class GCodeViewerData;
class GCodeViewInstance;
class GCodePanel;
class GCodePlayer
{
public:
    GCodePlayer();

    void set_panel(GCodePanel* panel);
    void set_instances(std::map<std::shared_ptr<GCodeViewInstance>, std::shared_ptr<GCodePanel>>& instances);
    void play();
    void stop();
    void render(int right, int bottom);

private:
    void action(bool restart = false);

private:
    std::map<std::shared_ptr<GCodeViewInstance>, std::shared_ptr<GCodePanel>> m_InstanceToPlaneMap;
    //IMSlider* m_moves_slider;
    //IMSlider* m_layers_slider;
    //GCodePanel* m_panel;

    bool m_play { false };
    std::chrono::high_resolution_clock::time_point m_last_time;
};

};
};
};

#endif