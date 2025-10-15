#include "DialogCommand.hpp"

#include "slic3r/GUI/Dialog/AboutDialog.hpp"
#include "slic3r/GUI/Dialog/KBShortcutsDialog.hpp"
#include "slic3r/GUI/Net/NetworkTestDialog.hpp"

namespace Slic3r {
namespace GUI {

void open_about_dialog(wxWindow* parent)
{
    AboutDialog dlg(parent);
    dlg.ShowModal();
}

void open_keyboard_shortcuts_dialog(wxWindow* parent)
{
    KBShortcutsDialog dlg(parent);
    dlg.ShowModal();
}

void open_network_test_dialog(wxWindow* parent)
{
    NetworkTestDialog dlg(parent);
    dlg.ShowModal();
}

}
}