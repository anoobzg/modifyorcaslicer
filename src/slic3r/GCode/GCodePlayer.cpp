#include "GCodePlayer.hpp"
#include "GCodePanel.hpp"
#include "GCodeViewInstance.hpp"
#include "slic3r/Render/ImGUI/IMSlider.hpp"
#include "slic3r/Render/ImGUI/ImGuiWrapper.hpp"

namespace Slic3r
{
    namespace GUI
    {
        namespace GCode
        {
            class GCodeViewerData;
            class GCodeViewInstance;

            GCodePlayer::GCodePlayer()
            {
            }

            void GCodePlayer::set_panel(GCodePanel *panel)
            {
                // m_panel = panel;
                // m_moves_slider = m_panel->get_moves_slider();
                // m_layers_slider = m_panel->get_layers_slider();
            }

            void GCodePlayer::set_instances(std::map<std::shared_ptr<GCodeViewInstance>, std::shared_ptr<GCodePanel>> &instances)
            {
                m_InstanceToPlaneMap = instances;
                // for (auto &item : m_InstanceToPlaneMap)
                // {
                //      auto &plane = item.second;
                //      int value = plane->get_layers_slider()->GetMinValue();
                //      plane->get_layers_slider()->SetHigherValue(value);
                // }
            }

            void GCodePlayer::play()
            {
                if (m_play == true)
                    return;

                m_play = true;
                action(true);
            }

            void GCodePlayer::stop()
            {
                m_play = false;
            }

            void GCodePlayer::render(int right, int bottom)
            {
                /* style and colors */
                ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowBorderSize, 0);
                ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_Text, ImGuiWrapper::COL_ORCA); // ORCA: Use orca color for slider value text

                int windows_flag = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

                // float scale = (float) app_em_unit() / 10.0f;

                ImVec2 size = ImVec2(40, 30);
                ImGui::SetNextWindowSize(size);
                ImGui::SetNextWindowPos(ImVec2(right, bottom));

                // ImGui::Begin("play_button", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
                ImGui::Begin("play_button", nullptr, (ImGuiWindowFlags)windows_flag);
                if (ImGui::Button("play", size))
                {
                    if (m_play)
                        stop();
                    else
                        play();
                }
                ImGui::End();

                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(2);

                if (m_play)
                    action();
            }

            void GCodePlayer::action(bool restart)
            {
                global_im_gui().set_requires_extra_frame();
                static double duration = 200;
                std::chrono::high_resolution_clock::time_point cur = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur - m_last_time).count();
                if (ms < duration)
                    return;

                m_last_time = cur;
                bool bFinished = true;
                for (auto &item : m_InstanceToPlaneMap)
                {
                    std::shared_ptr<GCodePanel> pPanel = item.second;
                    std::shared_ptr<GCodeViewInstance> pInstance = item.first;
                    auto layers_slider = pPanel->get_layers_slider();
                    auto moves_slider = pPanel->get_moves_slider();

                    if (restart)
                    {
                        // restart
                        const int first = layers_slider->GetMinValue();
                        layers_slider->SetHigherValue(first);
                        std::array<unsigned int, 2> range{static_cast<unsigned int>(layers_slider->GetLowerValue()),
                                                          static_cast<unsigned int>(layers_slider->GetHigherValue())};

                        pInstance->set_layers_z_range(range);
                        pPanel->update_moves_slider(false);
                        // update_moves_slider(false);
                        layers_slider->set_as_dirty(false);
                        moves_slider->SetHigherValue(moves_slider->GetMinValue());
                        moves_slider->set_as_dirty(true);
                    }
                    else
                    {
                        const int cur_move = moves_slider->GetHigherValue();
                        const int max_move = moves_slider->GetMaxValue();

                        if (cur_move < max_move)
                        {
                            const int next = cur_move + 1.0;
                            moves_slider->SetHigherValue(next);
                            moves_slider->set_as_dirty(true);
                            bFinished = false;
                        }
                        else
                        {
                            const int cur_layer = layers_slider->GetHigherValue();
                            const int max_layer = layers_slider->GetMaxValue();
                            if (cur_layer < max_layer)
                            {
                                const int next_layer = cur_layer + 1.0;
                                layers_slider->SetHigherValue(next_layer);

                                std::array<unsigned int, 2> range{static_cast<unsigned int>(layers_slider->GetLowerValue()),
                                                                  static_cast<unsigned int>(layers_slider->GetHigherValue())};

                                pInstance->set_layers_z_range(range);

                                pPanel->update_moves_slider(false);
                                // update_moves_slider(false);
                                layers_slider->set_as_dirty(false);
                                moves_slider->SetHigherValue(moves_slider->GetMinValue());
                                moves_slider->set_as_dirty(true);
                                bFinished = false;
                            }
                        }
                    }
                }
                if (!restart && bFinished)
                    stop();
            }

        }
    }
}
