#include "AppI18N.hpp"

#include "libslic3r/libslic3r.h"
#include "libslic3r/FileSystem/DataDir.hpp"

#include "slic3r/Config/AppConfig.hpp"
#include "slic3r/GUI/GUI.hpp"

namespace Slic3r {
namespace GUI {

std::unique_ptr<wxLocale> 	  m_wxLocale;
// System language, from locales, owned by wxWidgets.
const wxLanguageInfo		 *m_language_info_system = nullptr;
// Best translation language, provided by Windows or OSX, owned by wxWidgets.
const wxLanguageInfo		 *m_language_info_best   = nullptr;

#ifdef __linux__
static const wxLanguageInfo* linux_get_existing_locale_language(const wxLanguageInfo* language,
                                                                const wxLanguageInfo* system_language)
{
    constexpr size_t max_len = 50;
    char path[max_len] = "";
    std::vector<std::string> locales;
    const std::string lang_prefix = into_u8(language->CanonicalName.BeforeFirst('_'));

    // Call locale -a so we can parse the output to get the list of available locales
    // We expect lines such as "en_US.utf8". Pick ones starting with the language code
    // we are switching to. Lines with different formatting will be removed later.
    FILE* fp = popen("locale -a", "r");
    if (fp != NULL) {
        while (fgets(path, max_len, fp) != NULL) {
            std::string line(path);
            line = line.substr(0, line.find('\n'));
            if (boost::starts_with(line, lang_prefix))
                locales.push_back(line);
        }
        pclose(fp);
    }

    // locales now contain all candidates for this language.
    // Sort them so ones containing anything about UTF-8 are at the end.
    std::sort(locales.begin(), locales.end(), [](const std::string& a, const std::string& b)
    {
        auto has_utf8 = [](const std::string & s) {
            auto S = boost::to_upper_copy(s);
            return S.find("UTF8") != std::string::npos || S.find("UTF-8") != std::string::npos;
        };
        return ! has_utf8(a) && has_utf8(b);
    });

    // Remove the suffix behind a dot, if there is one.
    for (std::string& s : locales)
        s = s.substr(0, s.find("."));

    // We just hope that dear Linux "locale -a" returns country codes
    // in ISO 3166-1 alpha-2 code (two letter) format.
    // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
    // To be sure, remove anything not looking as expected
    // (any number of lowercase letters, underscore, two uppercase letters).
    locales.erase(std::remove_if(locales.begin(),
                                 locales.end(),
                                 [](const std::string& s) {
                                     return ! std::regex_match(s,
                                         std::regex("^[a-z]+_[A-Z]{2}$"));
                                 }),
                   locales.end());

    // Is there a candidate matching a country code of a system language? Move it to the end,
    // while maintaining the order of matches, so that the best match ends up at the very end.
    std::string temp_local = into_u8(system_language->CanonicalName.AfterFirst('_'));
    if (temp_local.size() >= 2) {
        temp_local = temp_local.substr(0, 2);
    }
    std::string system_country = "_" + temp_local;
    int cnt = locales.size();
    for (int i=0; i<cnt; ++i)
        if (locales[i].find(system_country) != std::string::npos) {
            locales.emplace_back(std::move(locales[i]));
            locales[i].clear();
        }

    // Now try them one by one.
    for (auto it = locales.rbegin(); it != locales.rend(); ++ it)
        if (! it->empty()) {
            const std::string &locale = *it;
            const wxLanguageInfo* lang = wxLocale::FindLanguageInfo(from_u8(locale));
            if (wxLocale::IsAvailable(lang->Language))
                return lang;
        }
    return language;
}
#endif

// Load gettext translation files and activate them at the start of the application,
// based on the "language" key stored in the application config.
bool app_load_language(wxString language, bool initial)
{
    BOOST_LOG_TRIVIAL(info) << boost::format("%1%: language %2%, initial: %3%") %__FUNCTION__ %language %initial;
    if (initial) {
    	// There is a static list of lookup path prefixes in wxWidgets. Add ours.
	    wxFileTranslationsLoader::AddCatalogLookupPathPrefix(from_u8(localization_dir()));
    	// Get the active language from PrusaSlicer.ini, or empty string if the key does not exist.
        language = app_get("language");
        if (! language.empty())
        	BOOST_LOG_TRIVIAL(info) << boost::format("language provided by OrcaSlicer.conf: %1%") % language;
        else {
            // Get the system language.
            const wxLanguage lang_system = wxLanguage(wxLocale::GetSystemLanguage());
            if (lang_system != wxLANGUAGE_UNKNOWN) {
                m_language_info_system = wxLocale::GetLanguageInfo(lang_system);
#ifdef __WXMSW__
                WCHAR wszLanguagesBuffer[LOCALE_NAME_MAX_LENGTH];
                ::LCIDToLocaleName(LOCALE_USER_DEFAULT, wszLanguagesBuffer, LOCALE_NAME_MAX_LENGTH, 0);
                wxString lang(wszLanguagesBuffer);
                lang.Replace('-', '_');
                if (auto info = wxLocale::FindLanguageInfo(lang))
                    m_language_info_system = info;
#endif
                BOOST_LOG_TRIVIAL(info) << boost::format("System language detected (user locales and such): %1%") % m_language_info_system->CanonicalName.ToUTF8().data();
                // BBS set language to app config
                app_set("language", m_language_info_system->CanonicalName.ToUTF8().data());
            } else {
                {
                    // Allocating a temporary locale will switch the default wxTranslations to its internal wxTranslations instance.
                    wxLocale temp_locale;
                    temp_locale.Init();
                    // Set the current translation's language to default, otherwise GetBestTranslation() may not work (see the wxWidgets source code).
                    wxTranslations::Get()->SetLanguage(wxLANGUAGE_DEFAULT);
                    // Let the wxFileTranslationsLoader enumerate all translation dictionaries for PrusaSlicer
                    // and try to match them with the system specific "preferred languages".
                    // There seems to be a support for that on Windows and OSX, while on Linuxes the code just returns wxLocale::GetSystemLanguage().
                    // The last parameter gets added to the list of detected dictionaries. This is a workaround
                    // for not having the English dictionary. Let's hope wxWidgets of various versions process this call the same way.
                    wxString best_language = wxTranslations::Get()->GetBestTranslation(SLIC3R_APP_KEY, wxLANGUAGE_ENGLISH);
                    if (!best_language.IsEmpty()) {
                        m_language_info_best = wxLocale::FindLanguageInfo(best_language);
                        BOOST_LOG_TRIVIAL(info) << boost::format("Best translation language detected (may be different from user locales): %1%") %
                                                        m_language_info_best->CanonicalName.ToUTF8().data();
                        app_set("language", m_language_info_best->CanonicalName.ToUTF8().data());
                    }
#ifdef __linux__
                    wxString lc_all;
                    if (wxGetEnv("LC_ALL", &lc_all) && !lc_all.IsEmpty()) {
                        // Best language returned by wxWidgets on Linux apparently does not respect LC_ALL.
                        // Disregard the "best" suggestion in case LC_ALL is provided.
                        m_language_info_best = nullptr;
                    }
#endif
                }
            }
        }
    }

	const wxLanguageInfo *language_info = language.empty() ? nullptr : wxLocale::FindLanguageInfo(language);
	if (! language.empty() && (language_info == nullptr || language_info->CanonicalName.empty())) {
		// Fix for wxWidgets issue, where the FindLanguageInfo() returns locales with undefined ANSII code (wxLANGUAGE_KONKANI or wxLANGUAGE_MANIPURI).
		language_info = nullptr;
    	BOOST_LOG_TRIVIAL(error) << boost::format("Language code \"%1%\" is not supported") % language.ToUTF8().data();
	}

	if (language_info != nullptr && language_info->LayoutDirection == wxLayout_RightToLeft) {
    	BOOST_LOG_TRIVIAL(trace) << boost::format("The following language code requires right to left layout, which is not supported: %1%") % language_info->CanonicalName.ToUTF8().data();
		language_info = nullptr;
	}

    if (language_info == nullptr) {
        // PrusaSlicer does not support the Right to Left languages yet.
        if (m_language_info_system != nullptr && m_language_info_system->LayoutDirection != wxLayout_RightToLeft)
            language_info = m_language_info_system;
        if (m_language_info_best != nullptr && m_language_info_best->LayoutDirection != wxLayout_RightToLeft)
        	language_info = m_language_info_best;
	    if (language_info == nullptr)
			language_info = wxLocale::GetLanguageInfo(wxLANGUAGE_ENGLISH_US);
    }

	BOOST_LOG_TRIVIAL(trace) << boost::format("Switching wxLocales to %1%") % language_info->CanonicalName.ToUTF8().data();

    // Select language for locales. This language may be different from the language of the dictionary.
    //if (language_info == m_language_info_best || language_info == m_language_info_system) {
    //    // The current language matches user's default profile exactly. That's great.
    //} else if (m_language_info_best != nullptr && language_info->CanonicalName.BeforeFirst('_') == m_language_info_best->CanonicalName.BeforeFirst('_')) {
    //    // Use whatever the operating system recommends, if it the language code of the dictionary matches the recommended language.
    //    // This allows a Swiss guy to use a German dictionary without forcing him to German locales.
    //    language_info = m_language_info_best;
    //} else if (m_language_info_system != nullptr && language_info->CanonicalName.BeforeFirst('_') == m_language_info_system->CanonicalName.BeforeFirst('_'))
    //    language_info = m_language_info_system;

    // Alternate language code.
    wxLanguage language_dict = wxLanguage(language_info->Language);
    if (language_info->CanonicalName.BeforeFirst('_') == "sk") {
    	// Slovaks understand Czech well. Give them the Czech translation.
    	language_dict = wxLANGUAGE_CZECH;
		BOOST_LOG_TRIVIAL(info) << "Using Czech dictionaries for Slovak language";
    }

#ifdef __linux__
    // If we can't find this locale , try to use different one for the language
    // instead of just reporting that it is impossible to switch.
    if (! wxLocale::IsAvailable(language_info->Language) && m_language_info_system) {
        std::string original_lang = into_u8(language_info->CanonicalName);
        language_info = linux_get_existing_locale_language(language_info, m_language_info_system);
        BOOST_LOG_TRIVIAL(info) << boost::format("Can't switch language to %1% (missing locales). Using %2% instead.")
                                    % original_lang % language_info->CanonicalName.ToUTF8().data();
    }
#endif

    if (! wxLocale::IsAvailable(language_info->Language)&&initial) {
        language_info = wxLocale::GetLanguageInfo(wxLANGUAGE_ENGLISH_UK);
        app_set("language", language_info->CanonicalName.ToUTF8().data());
    }
    else if (initial) {
        // bbs supported languages
    }

    if (! wxLocale::IsAvailable(language_info->Language)) {
    	// Loading the language dictionary failed.
    	wxString message = "Switching Orca Slicer to language " + language_info->CanonicalName + " failed.";
#if !defined(_WIN32) && !defined(__APPLE__)
        // likely some linux system
        message += "\nYou may need to reconfigure the missing locales, likely by running the \"locale-gen\" and \"dpkg-reconfigure locales\" commands.\n";
#endif
        if (initial)
        	message + "\n\nApplication will close.";
        wxMessageBox(message, "Switching language failed", wxOK | wxICON_ERROR);
        if (initial)
			std::exit(EXIT_FAILURE);
		else
			return false;
    }

    // Release the old locales, create new locales.
    //FIXME wxWidgets cause havoc if the current locale is deleted. We just forget it causing memory leaks for now.
    m_wxLocale.release();
    m_wxLocale = Slic3r::make_unique<wxLocale>();
    m_wxLocale->Init(language_info->Language);
    // Override language at the active wxTranslations class (which is stored in the active m_wxLocale)
    // to load possibly different dictionary, for example, load Czech dictionary for Slovak language.
    wxTranslations::Get()->SetLanguage(language_dict);
    m_wxLocale->AddCatalog(SLIC3R_APP_KEY);

    return true;
}

void app_uninit_language()
{
    m_wxLocale.reset();
}

wxString        app_current_language_code()
{
    if(!m_wxLocale)
        return wxString("");
    return m_wxLocale->GetCanonicalName();
}

wxString 		app_current_language_code_safe()
{
	// Translate the language code to a code, for which Prusa Research maintains translations.
	const std::map<wxString, wxString> mapping {
		{ "cs", 	"cs_CZ", },
		{ "sk", 	"cs_CZ", },
		{ "de", 	"de_DE", },
		{ "nl", 	"nl_NL", },
		{ "sv", 	"sv_SE", },
		{ "es", 	"es_ES", },
		{ "fr", 	"fr_FR", },
		{ "it", 	"it_IT", },
		{ "ja", 	"ja_JP", },
		{ "ko", 	"ko_KR", },
		{ "pl", 	"pl_PL", },
		{ "uk", 	"uk_UA", },
		{ "zh", 	"zh_CN", },
		{ "ru", 	"ru_RU", },
        { "tr", 	"tr_TR", },
        { "pt", 	"pt_BR", },
	};
	wxString language_code = app_current_language_code().BeforeFirst('_');
	auto it = mapping.find(language_code);
	if (it != mapping.end())
		language_code = it->second;
	else
		language_code = "en_US";
	return language_code;
}

bool            app_is_localized()
{ 
    if(!m_wxLocale)
        return false;
    return m_wxLocale->GetLocale() != "English"; }
}
}