#ifndef FONT_HPP_THEME
#define FONT_HPP_THEME
#include <wx/stattext.h>

namespace Slic3r {
namespace GUI {

void initSysFont();
wxFont sysFont(int size, bool bold = false);

namespace Font {
    extern wxFont Head_48;
    extern wxFont Head_32;
	extern wxFont Head_24;
	extern wxFont Head_20;
	extern wxFont Head_18;
	extern wxFont Head_16;
	extern wxFont Head_15;
	extern wxFont Head_14;
	extern wxFont Head_13;
	extern wxFont Head_12;
	extern wxFont Head_11;
    extern wxFont Head_10;

	extern wxFont Body_16;
	extern wxFont Body_15;
	extern wxFont Body_14;
    extern wxFont Body_13;
	extern wxFont Body_12;
	extern wxFont Body_10;
	extern wxFont Body_11;
	extern wxFont Body_9;
	extern wxFont Body_8;
}

}
}
#endif // FONT_HPP_THEME