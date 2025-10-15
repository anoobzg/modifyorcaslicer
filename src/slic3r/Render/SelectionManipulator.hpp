#ifndef _slic3r_SelectionManipulator_hpp
#define _slic3r_SelectionManipulator_hpp


namespace Slic3r {
class Model;
class GLVolumeCollection;
namespace GUI {

class Selection;
class SelectionManipulator
{
public:
    SelectionManipulator(Model* model, Selection* selection, GLVolumeCollection* volumes);

    void set_model(Model* model);
    void set_selection(Selection* selection);
    void set_volumes(GLVolumeCollection* volumes);

    void do_move(const std::string& snapshot_type);
    void do_rotate(const std::string& snapshot_type);
    void do_scale(const std::string& snapshot_type);
    void do_center();
    void do_drop();
    void do_center_plate(const int plate_idx);
    // void do_mirror(const std::string& snapshot_type);

private:
    Model* m_model;
    Selection* m_selection;
    GLVolumeCollection* m_volumes;
};

};
};

#endif