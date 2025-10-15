#pragma once 

namespace Slic3r {
namespace GUI {

#define AUFILE_GREY700 wxColour(107, 107, 107)
#define AUFILE_GREY500 wxColour(158, 158, 158)
#define AUFILE_GREY300 wxColour(238, 238, 238)
#define AUFILE_GREY200 wxColour(248, 248, 248)
#define AUFILE_BRAND wxColour(0, 150, 136)
#define AUFILE_BRAND_TRANSPARENT wxColour("#E5F0EE") // ORCA color with %10 opacity
//#define AUFILE_PICTURES_SIZE wxSize(FromDIP(300), FromDIP(300))
//#define AUFILE_PICTURES_PANEL_SIZE wxSize(FromDIP(300), FromDIP(340))
#define AUFILE_PICTURES_SIZE wxSize(FromDIP(168), FromDIP(168))
#define AUFILE_PICTURES_PANEL_SIZE wxSize(FromDIP(168), FromDIP(208))
#define AUFILE_SIZE wxSize(FromDIP(168), FromDIP(168))
#define AUFILE_PANEL_SIZE wxSize(FromDIP(168), FromDIP(208))
#define AUFILE_TEXT_HEIGHT FromDIP(40)
#define AUFILE_ROUNDING FromDIP(5)
    
}
}