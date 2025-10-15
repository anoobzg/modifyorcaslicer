#include "PresetLoad.hpp"

#include "libslic3r/FileSystem/FileHelp.hpp"
#include "libslic3r/FileSystem/DataDir.hpp"

using namespace nlohmann;
namespace Slic3r{
    int get_filament_info(const std::string& VendorDirectory, const json& pFilaList, const std::string& filepath, std::string &sVendor, std::string &sType, const std::string& default_path)
    {
        //GetStardardFilePath(filepath);
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " get_filament_info:VendorDirectory - " << VendorDirectory << ", Filepath - "<<filepath;

        try {
            std::string contents;
            Slic3r::Utils::load_file_content(filepath, contents);
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ": Json Contents: " << contents;
            json jLocal = json::parse(contents);

            if (sVendor == "") {
                if (jLocal.contains("filament_vendor"))
                    sVendor = jLocal["filament_vendor"][0];
                else {
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << filepath << " - Not Contains filament_vendor";
                }
            }

            if (sType == "") {
                if (jLocal.contains("filament_type"))
                    sType = jLocal["filament_type"][0];
                else {
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << filepath << " - Not Contains filament_type";
                }
            }

            if (sVendor == "" || sType == "")
            {
                if (jLocal.contains("inherits")) {
                    std::string FName = jLocal["inherits"];

                    if (!pFilaList.contains(FName)) { 
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "pFilaList - Not Contains inherits filaments: " << FName;
                        return -1; 
                    }

                    std::string FPath = pFilaList[FName]["sub_path"];
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " Before Format Inherits Path: VendorDirectory - " << VendorDirectory << ", sub_path - " << FPath;

                    std::string strNewFile = (boost::format("%1%/%2%")%VendorDirectory%FPath).str();
                    boost::filesystem::path inherits_path(strNewFile);
                    if (!boost::filesystem::exists(inherits_path))
                        inherits_path = (boost::filesystem::path(default_path) / boost::filesystem::path(FPath)).make_preferred();

                    //boost::filesystem::path nf(strNewFile.c_str());
                    if (boost::filesystem::exists(inherits_path))
                        return get_filament_info(VendorDirectory, pFilaList, inherits_path.string(), sVendor, sType);
                    else {
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " inherits File Not Exist: " << inherits_path;
                        return -1;
                    }
                } else {
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << filepath << " - Not Contains inherits";
                    if (sType == "") {
                        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "sType is Empty";
                        return -1;
                    }
                    else
                        sVendor = "Generic";
                        return 0;
                }
            }
            else
                return 0;
        }
        catch(nlohmann::detail::parse_error &err) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse "<<filepath <<" got a nlohmann::detail::parse_error, reason = " << err.what();
            return -1;
        }
        catch (std::exception &e)
        {
            // wxLogMessage("GUIDE: load_profile_error  %s ", e.what());
            // wxMessageBox(e.what(), "", MB_OK);
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__<< ": parse " << filepath <<" got exception: "<<e.what();
            return -1;
        }

        return 0;
    }

    int load_profile_family(const std::string& strVendor, const std::string& strFilePath, nlohmann::json& result_json)
    {
        boost::filesystem::path file_path(strFilePath);
        boost::filesystem::path vendor_dir = boost::filesystem::absolute(file_path.parent_path() / strVendor).make_preferred();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  vendor path %1%.") % vendor_dir.string();
        try {
            std::string contents;
            Slic3r::Utils::load_file_content(strFilePath, contents);
            json jLocal = json::parse(contents);

            // BBS:models
            json pmodels = jLocal["machine_model_list"];
            int  nsize   = pmodels.size();

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  got %1% machine models") % nsize;

            for (int n = 0; n < nsize; n++) {
                json OneModel = pmodels.at(n);

                OneModel["model"] = OneModel["name"];
                OneModel.erase("name");

                std::string s1 = OneModel["model"];
                std::string s2 = OneModel["sub_path"];

                boost::filesystem::path sub_path = boost::filesystem::absolute(vendor_dir / s2).make_preferred();
                std::string             sub_file = sub_path.string();

                Slic3r::Utils::load_file_content(sub_file, contents);
                json pm = json::parse(contents);

                OneModel["vendor"]    = strVendor;
                std::string NozzleOpt = pm["nozzle_diameter"];
                Slic3r::Utils::StrReplace(NozzleOpt, " ", "");
                OneModel["nozzle_diameter"] = NozzleOpt;
                OneModel["materials"]       = pm["default_materials"];

                // wxString strCoverPath = wxString::Format("%s\\%s\\%s_cover.png", strFolder, strVendor, std::string(s1.mb_str()));
                std::string             cover_file = s1 + "_cover.png";
                boost::filesystem::path cover_path = boost::filesystem::absolute(boost::filesystem::path(resources_dir()) / "/profiles/" / strVendor / cover_file).make_preferred();
                if (!boost::filesystem::exists(cover_path)) {
                    cover_path =
                        (boost::filesystem::absolute(boost::filesystem::path(resources_dir()) / "/web/image/printer/") /
                        cover_file)
                            .make_preferred();
                }
                OneModel["cover"]                  = cover_path.string();

                OneModel["nozzle_selected"] = "";

                result_json["model"].push_back(OneModel);
            }

            // BBS:Machine
            json pmachine = jLocal["machine_list"];
            nsize         = pmachine.size();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  got %1% machines") % nsize;
            for (int n = 0; n < nsize; n++) {
                json OneMachine = pmachine.at(n);

                std::string s1 = OneMachine["name"];
                std::string s2 = OneMachine["sub_path"];

                boost::filesystem::path sub_path = boost::filesystem::absolute(vendor_dir / s2).make_preferred();
                std::string             sub_file = sub_path.string();

                Slic3r::Utils::load_file_content(sub_file, contents);
                json pm = json::parse(contents);

                std::string strInstant = pm["instantiation"];
                if (strInstant.compare("true") == 0) {
                    OneMachine["model"] = pm["printer_model"];
                    OneMachine["nozzle"] = pm["nozzle_diameter"][0];

                    result_json["machine"][s1]=OneMachine;
                }
            }

            // BBS:Filament
            json pFilament = jLocal["filament_list"];
            json tFilaList;
            nsize          = pFilament.size();

            for (int n = 0; n < nsize; n++) {
                json OneFF = pFilament.at(n);

                std::string s1    = OneFF["name"];
                std::string s2    = OneFF["sub_path"];

                tFilaList[s1] = OneFF;
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Vendor: " << strVendor <<", tFilaList Add: " << s1;
            }

            int nFalse  = 0;
            int nModel  = 0;
            int nFinish = 0;
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  got %1% filaments") % nsize;
            for (int n = 0; n < nsize; n++) {
                json OneFF = pFilament.at(n);

                std::string s1 = OneFF["name"];
                std::string s2 = OneFF["sub_path"];

                if (!result_json["filament"].contains(s1)) {
                    // wxString ModelFilePath = wxString::Format("%s\\%s\\%s", strFolder, strVendor, s2);
                    boost::filesystem::path sub_path = boost::filesystem::absolute(vendor_dir / s2).make_preferred();
                    std::string             sub_file = sub_path.string();
                    Slic3r::Utils::load_file_content(sub_file, contents);
                    json pm = json::parse(contents);
                    
                    std::string strInstant = pm["instantiation"];
                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Load Filament:" << s1 << ",Path:" << sub_file << ",instantiation?" << strInstant;

                    if (strInstant == "true") {
                        std::string sV;
                        std::string sT;

                        int nRet = get_filament_info(vendor_dir.string(), tFilaList, sub_file, sV, sT);
                        if (nRet != 0) { 
                            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Load Filament:" << s1 << ",get_filament_info Failed, Vendor:" << sV << ",Type:"<< sT;
                            continue; 
                        }

                        OneFF["vendor"] = sV;
                        OneFF["type"]   = sT;

                        OneFF["models"]   = "";

                        json pPrinters = pm["compatible_printers"];
                        int nPrinter   = pPrinters.size();
                        std::string ModelList = "";
                        for (int i = 0; i < nPrinter; i++)
                        {
                            std::string sP = pPrinters.at(i);
                            if (result_json["machine"].contains(sP))
                            {
                                std::string mModel = result_json["machine"][sP]["model"];
                                std::string mNozzle = result_json["machine"][sP]["nozzle"];
                                std::string NewModel = mModel + "++" + mNozzle;

                                ModelList = (boost::format("%1%[%2%]") % ModelList % NewModel).str();
                            }
                        }

                        OneFF["models"]    = ModelList;
                        OneFF["selected"] = 0;

                        result_json["filament"][s1] = OneFF;
                    } else
                        continue;

                }
            }

            // process
            json pProcess = jLocal["process_list"];
            nsize         = pProcess.size();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(",  got %1% processes") % nsize;
            for (int n = 0; n < nsize; n++) {
                json OneProcess = pProcess.at(n);

                std::string s2 = OneProcess["sub_path"];
                // wxString ModelFilePath = wxString::Format("%s\\%s\\%s", strFolder, strVendor, s2);
                boost::filesystem::path sub_path = boost::filesystem::absolute(vendor_dir / s2).make_preferred();
                std::string             sub_file = sub_path.string();
                Slic3r::Utils::load_file_content(sub_file, contents);
                json pm = json::parse(contents);

                std::string bInstall = pm["instantiation"];
                if (bInstall == "true") { result_json["process"].push_back(OneProcess); }
            }

        } catch (nlohmann::detail::parse_error &err) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << strFilePath << " got a nlohmann::detail::parse_error, reason = " << err.what();
            return -1;
        } catch (std::exception &e) {
            // wxMessageBox(e.what(), "", MB_OK);
            // wxLogMessage("GUIDE: LoadFamily Error: %s", e.what());
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << strFilePath << " got exception: " << e.what();
            return -1;
        }

        return 0;        
    }
}