#ifndef _slic3r_3DBedTexture_hpp
#define _slic3r_3DBedTexture_hpp

#include "GLTexture.hpp"
#include "GLModel.hpp"
#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {
namespace GUI {

class Bed3DTexture
{
public:
    class BedTextureInfo {
    public:
        class TexturePart {
        public:
            // position
            float x;
            float y;
            float w;
            float h;
            std::string filename;
            GLTexture* texture { nullptr };
            Vec2d offset;
            GLModel* buffer { nullptr };
            TexturePart(float xx, float yy, float ww, float hh, std::string file){
                x = xx; y = yy;
                w = ww; h = hh;
                filename = file;
                texture = nullptr;
                buffer = nullptr;
                offset = Vec2d(0, 0);
            }

            TexturePart(const TexturePart& part) {
                this->x = part.x;
                this->y = part.y;
                this->w = part.w;
                this->h = part.h;
                this->offset = part.offset;
                this->buffer    = part.buffer;
                this->filename  = part.filename;
                this->texture   = part.texture;
            }

            void update_buffer();
            void reset();
        };
        std::vector<TexturePart> parts;
        void                     reset();
    };


public:
    Bed3DTexture();
    ~Bed3DTexture();

    void set_dark_mode(bool is_dark);

    const std::string &get_logo_texture_filename();
    void update_logo_texture_filename(const std::string &texture_filename);
    void generate_icon_textures();
    void release_icon_textures();
    void set_render_option(bool bedtype_texture, bool plate_settings, bool cali);

    void load_bedtype_textures();
    void load_cali_textures();
    
private:
    void init_bed_type_info();
    void update_bed_type_info(const std::vector<Pointfs>& shape_group); 
    void init_cali_texture_info();


public:
    static const unsigned int MAX_PLATES_COUNT = 36;
    static GLTexture bed_textures[(unsigned int)btCount];
    bool is_load_bedtype_textures;
    bool is_load_cali_texture;
    
    std::string m_logo_texture_filename;
    GLTexture m_logo_texture;
    GLTexture m_del_texture;
    GLTexture m_del_hovered_texture;
    GLTexture m_move_front_hovered_texture;
    GLTexture m_move_front_texture;
    GLTexture m_arrange_texture;
    GLTexture m_arrange_hovered_texture;
    GLTexture m_orient_texture;
    GLTexture m_orient_hovered_texture;
    GLTexture m_locked_texture;
    GLTexture m_locked_hovered_texture;
    GLTexture m_lockopen_texture;
    GLTexture m_lockopen_hovered_texture;
    GLTexture m_plate_settings_texture;
    GLTexture m_plate_settings_changed_texture;
    GLTexture m_plate_settings_hovered_texture;
    GLTexture m_plate_settings_changed_hovered_texture;
    GLTexture m_plate_name_edit_texture;
    GLTexture m_plate_name_edit_hovered_texture;
    GLTexture m_idx_textures[MAX_PLATES_COUNT];

    BedTextureInfo bed_texture_info[btCount];
    BedTextureInfo cali_texture_info;

    bool render_bedtype_logo = true;
    bool render_plate_settings = true;
    bool render_cali_logo = true;

    bool m_is_dark;

private:
    bool m_need_generate;

};


};
};

#endif