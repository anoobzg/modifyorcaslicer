#pragma once

namespace Slic3r {
namespace GUI {

enum ItemType {
    itUndef         = 0,
    itPlate         = 1,
    itObject        = 2,
    itVolume        = 4,
    itInstanceRoot  = 8,
    itInstance      = 16,
    itSettings      = 32,
    itLayerRoot     = 64,
    itLayer         = 128,
    itInfo          = 256,
};

enum ColumnNumber
{
    colName         = 0,    // item name
    colPrint           ,    // printable property
    colFilament        ,    // extruder selection
    // BBS
    colSupportPaint    ,
    colColorPaint      ,
    colSinking         ,
    colEditing         ,    // item editing
    colCount           ,
};

enum PrintIndicator
{
    piUndef         = 0,    // no print indicator
    piPrintable        ,    // printable
    piUnprintable      ,    // unprintable
};

enum class InfoItemType
{
    Undef,
    CustomSupports,
    //CustomSeam,
    MmuSegmentation,
    //Sinking
    CutConnectors,
};

struct ItemForDelete
{
    ItemType    type;
    int         obj_idx;
    int         sub_obj_idx;

    ItemForDelete(ItemType type, int obj_idx, int sub_obj_idx)
        : type(type), obj_idx(obj_idx), sub_obj_idx(sub_obj_idx)
    {}

    bool operator==(const ItemForDelete& r) const
    {
        return (type == r.type && obj_idx == r.obj_idx && sub_obj_idx == r.sub_obj_idx);
    }

    bool operator<(const ItemForDelete& r) const
    {
        if (obj_idx != r.obj_idx)
            return (obj_idx < r.obj_idx);
        return (sub_obj_idx < r.sub_obj_idx);
    }
};

}
}