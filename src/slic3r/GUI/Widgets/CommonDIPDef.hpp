#pragma once 

namespace Slic3r {
namespace GUI {

#undef  ICON_SIZE
#define ICON_SIZE               wxSize(FromDIP(16), FromDIP(16))
#define TABLE_BORDER            FromDIP(28)
#define HEADER_VERT_PADDING     FromDIP(12)
#define HEADER_BEG_PADDING      FromDIP(30)
#define ICON_GAP                FromDIP(44)
#define HEADER_END_PADDING      FromDIP(24)
#define ROW_VERT_PADDING        FromDIP(6)
#define ROW_BEG_PADDING         FromDIP(20)
#define EDIT_BOXES_GAP          FromDIP(30)
#define ROW_END_PADDING         FromDIP(21)
#define BTN_SIZE                wxSize(FromDIP(58), FromDIP(24))
#define BTN_GAP                 FromDIP(20)
#define TEXT_BEG_PADDING        FromDIP(30)
#define MAX_FLUSH_VALUE         999
#define MIN_WIPING_DIALOG_WIDTH FromDIP(300)
#define TIP_MESSAGES_PADDING    FromDIP(8)
    
}}