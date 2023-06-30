#include <vitasdk.h>
#include <string>
#include <cstring>
#include <map>

#include "clients/remote_client.h"
#include "config.h"
#include "fs.h"
#include "lang.h"

extern "C"
{
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
std::vector<std::string> http_servers;
std::map<std::string, RemoteSettings> site_settings;
bool warn_missing_installs;
bool show_hidden_files;

namespace CONFIG
{

    void LoadConfig()
    {
        const char *bg_music_list_str;

        if (!FS::FolderExists(DATA_PATH))
        {
            FS::MkDirs(DATA_PATH);
        }

        sites = {"Site 1", "Site 2", "Site 3", "Site 4", "Site 5", "Site 6", "Site 7", "Site 8", "Site 9", "Site 10"};
        http_servers = {HTTP_SERVER_APACHE, HTTP_SERVER_MS_IIS, HTTP_SERVER_NGINX, HTTP_SERVER_NPX_SERVE};

        OpenIniFile(CONFIG_INI_FILE);

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

        show_hidden_files = ReadBool(CONFIG_GLOBAL, CONFIG_SHOW_HIDDEN_FILES, false);
        WriteBool(CONFIG_GLOBAL, CONFIG_SHOW_HIDDEN_FILES, show_hidden_files);

        for (int i = 0; i < sites.size(); i++)
        {
            RemoteSettings setting;
            sprintf(setting.site_name, "%s", sites[i].c_str());

            sprintf(setting.server, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER, setting.server);

            sprintf(setting.username, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, setting.username);

            sprintf(setting.password, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, setting.password);

            setting.gg_account.token_expiry = ReadLong(sites[i].c_str(), CONFIG_GOOGLE_TOKEN_EXPIRY, 0);
            WriteLong(sites[i].c_str(), CONFIG_GOOGLE_TOKEN_EXPIRY, setting.gg_account.token_expiry);

            sprintf(setting.gg_account.access_token, "%s", ReadString(sites[i].c_str(), CONFIG_GOOGLE_ACCESS_TOKEN, ""));
            WriteString(sites[i].c_str(), CONFIG_GOOGLE_ACCESS_TOKEN, setting.gg_account.access_token);

            sprintf(setting.gg_account.refresh_token, "%s", ReadString(sites[i].c_str(), CONFIG_GOOGLE_REFRESH_TOKEN, ""));
            WriteString(sites[i].c_str(), CONFIG_GOOGLE_REFRESH_TOKEN, setting.gg_account.refresh_token);

            SetClientType(&setting);
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
        float ver = atof(app_ver) / 100;
        sprintf(app_ver, "%.2f", ver);
    }

    void SaveConfig()
    {
        OpenIniFile(CONFIG_INI_FILE);

        WriteString(last_site, CONFIG_REMOTE_SERVER, remote_settings->server);
        WriteString(last_site, CONFIG_REMOTE_SERVER_USER, remote_settings->username);
        WriteString(last_site, CONFIG_REMOTE_SERVER_PASSWORD, remote_settings->password);
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);
        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void SetClientType(RemoteSettings *setting)
    {
        if (strncmp(setting->server, "smb://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_SMB;
        }
        else if (strncmp(setting->server, "ftp://", 6) == 0 || strncmp(setting->server, "sftp://", 7) == 0)
        {
            setting->type = CLIENT_TYPE_FTP;
        }
        else if (strncmp(setting->server, "webdav://", 9) == 0 || strncmp(setting->server, "webdavs://", 10) == 0)
        {
            setting->type = CLIENT_TYPE_WEBDAV;
        }
        else if (strncmp(setting->server, "https://drive.google.com", 24) == 0)
        {
            setting->type = CLIENT_TYPE_GOOGLE;
        }
        else if (strncmp(setting->server, "http://", 7) == 0 || strncmp(setting->server, "https://", 8) == 0)
        {
            setting->type = CLIENT_TYPE_HTTP_SERVER;
        }
        else
        {
            setting->type = CLINET_TYPE_UNKNOWN;
        }
    }
}
