#include <vitasdk.h>
#include <string>
#include <cstring>
#include <map>

#include "config.h"
#include "fs.h"
#include "lang.h"

extern "C" {
	#include "inifile.h"
}

bool swap_xo;
std::vector<std::string> bg_music_list;
bool enable_backgrou_music;
RemoteSettings *remote_settings;
RemoteClient *client;
char local_directory[MAX_PATH_LENGTH];
char remote_directory[MAX_PATH_LENGTH];
char app_ver[6];
char last_site[32];
char display_site[64];
char language[32];
std::vector<std::string> sites;
std::map<std::string,RemoteSettings> site_settings;
bool warn_missing_installs;

namespace CONFIG {

    void LoadConfig()
    {
        const char* bg_music_list_str;

        if (!FS::FolderExists(DATA_PATH))
        {
            FS::MkDirs(DATA_PATH);
        }

        sites = { "Site 1", "Site 2", "Site 3", "Site 4", "Site 5", "Site 6", "Site 7", "Site 8", "Site 9"};

		OpenIniFile (CONFIG_INI_FILE);

        // Load global config
        swap_xo = ReadBool(CONFIG_GLOBAL, CONFIG_SWAP_XO, false);
        WriteBool(CONFIG_GLOBAL, CONFIG_SWAP_XO, swap_xo);

        sprintf(language, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LANGUAGE, ""));
        WriteString(CONFIG_GLOBAL, CONFIG_LANGUAGE, language);

        sprintf(local_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, "ux0:"));
        WriteString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, local_directory);

        sprintf(remote_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, remote_directory);

        warn_missing_installs = ReadBool(CONFIG_GLOBAL, CONFIG_UPDATE_WARN_MISSING, true);
        WriteBool(CONFIG_GLOBAL, CONFIG_UPDATE_WARN_MISSING, warn_missing_installs);

        for (int i=0; i <sites.size(); i++)
        {
            RemoteSettings setting;
            sprintf(setting.site_name, "%s", sites[i].c_str());

            sprintf(setting.server, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER, setting.server);

            sprintf(setting.username, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, setting.username);

            sprintf(setting.password, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, setting.password);

            site_settings.insert(std::make_pair(sites[i], setting));
        }

        sprintf(last_site, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LAST_SITE, sites[0].c_str()));
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);

        remote_settings = &site_settings[std::string(last_site)];

        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();

        void *f = FS::OpenRead(REMOTE_CLIENT_VERSION_PATH);
        memset(app_ver, 0, sizeof(app_ver));
        FS::Read(f, app_ver, 3);
        FS::Close(f);
        float ver = atof(app_ver)/100;
        sprintf(app_ver, "%.2f", ver);
    }

    void SaveConfig()
    {
		OpenIniFile (CONFIG_INI_FILE);

        WriteString(last_site, CONFIG_REMOTE_SERVER, remote_settings->server);
        WriteString(last_site, CONFIG_REMOTE_SERVER_USER, remote_settings->username);
        WriteString(last_site, CONFIG_REMOTE_SERVER_PASSWORD, remote_settings->password);
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);
        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void ParseMultiValueString(const char* prefix_list, std::vector<std::string> &prefixes, bool toLower)
    {
        std::string prefix = "";
        int length = strlen(prefix_list);
        for (int i=0; i<length; i++)
        {
            char c = prefix_list[i];
            if (c != ' ' && c != '\t' && c != ',')
            {
                if (toLower)
                {
                    prefix += std::tolower(c);
                }
                else
                {
                    prefix += c;
                }
            }
            
            if (c == ',' || i==length-1)
            {
                prefixes.push_back(prefix);
                prefix = "";
            }
        }
    }

    std::string GetMultiValueString(std::vector<std::string> &multi_values)
    {
        std::string vts = std::string("");
        if (multi_values.size() > 0)
        {
            for (int i=0; i<multi_values.size()-1; i++)
            {
                vts.append(multi_values[i]).append(",");
            }
            vts.append(multi_values[multi_values.size()-1]);
        }
        return vts;
    }

    void RemoveFromMultiValues(std::vector<std::string> &multi_values, std::string value)
    {
        auto itr = std::find(multi_values.begin(), multi_values.end(), value);
        if (itr != multi_values.end()) multi_values.erase(itr);
    }
}
