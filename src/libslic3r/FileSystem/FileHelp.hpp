#ifndef file_help_hpp_
#define file_help_hpp_
#include <string>

#define STL_SVG_MAX_FILE_SIZE_MB 3
namespace Slic3r {
   namespace Utils {

      bool is_file_too_large(std::string file_path, bool &try_ok);
      void slash_to_back_slash(std::string& file_path);// "//" to "\"

      bool load_file_content(const std::string& jPath, std::string& content);
      void StrReplace(std::string &strBase, std::string strSrc, std::string strDes);
   }

   enum FileType
   {
      FT_STEP,
      FT_STL,
      FT_OBJ,
      FT_AMF,
      FT_3MF,
      FT_GCODE_3MF,
      FT_GCODE,
      FT_MODEL,
      FT_ZIP,
      FT_PROJECT,
      FT_GALLERY,

      FT_INI,
      FT_SVG,

      FT_TEX,

      FT_SL1,

      FT_SIZE,
   };

   std::string file_wildcards(FileType file_type, const std::string &custom_extension = std::string{});

   void read_binary_stl(const std::string& filename, std::string& model_id, std::string& code);
}
#endif // file_help_hpp_
