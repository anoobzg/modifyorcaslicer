#pragma once

namespace Slic3r {
namespace GUI {
    
// Discard and Cancel buttons are always but next buttons are optional
enum ActionButtons {
    TRANSFER = 1,
    KEEP = 2,
    SAVE = 4,
    DONT_SAVE = 8,
    REMEMBER_CHOISE = 0x10000
};

enum class Action {
    Undef,
    Transfer,
    Discard,
    Save
};

}
}