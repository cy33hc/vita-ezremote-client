#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include "common.h"
#include "fs.h"
#include "clients/remote_client.h"

#define APP_ID "RMTCLI001"
#define DATA_PATH "ux0:data/" APP_ID
#define APP_PATH "ux0:app/" APP_ID
#define CONFIG_INI_FILE DATA_PATH "/config.ini"
#define TMP_EDITOR_FILE DATA_PATH "/tmp_editor.txt"
#define CACERT_FILE APP_PATH "/certs/cacert.pem"

#define REMOTE_CLIENT_VPK_URL "https://github.com/cy33hc/vita-ezremote-client/releases/download/latest/ezremoteclient.vpk"
#define REMOTE_CLIENT_VERSION_URL "https://github.com/cy33hc/vita-ezremote-client/releases/download/latest/version.txt"
#define REMOTE_CLIENT_VERSION_PATH APP_PATH "/version.txt"
#define REMOTE_CLIENT_VERSION_UPDATE_PATH DATA_PATH "/tmp/version.txt"
#define REMOTE_CLIENT_VPK_UPDATE_PATH DATA_PATH "/tmp/ezremoteclient.vpk"

#define CONFIG_GLOBAL "Global"

#define CONFIG_SHOW_HIDDEN_FILES "show_hidden_files"
#define CONFIG_DEFAULT_STYLE_NAME "Default"
#define CONFIG_SWAP_XO "swap_xo"
#define CONFIG_MAX_EDIT_FILE_SIZE "max_edit_file_size"

#define CONFIG_BACKGROUD_MUSIC "backgroud_music"
#define CONFIG_ENABLE_BACKGROUND_MUSIC "enable_backgroud_music"

#define CONFIG_REMOTE_SERVER_NAME "remote_server_name"
#define CONFIG_REMOTE_SERVER "remote_server"
#define CONFIG_REMOTE_SERVER_USER "remote_server_user"
#define CONFIG_REMOTE_SERVER_PASSWORD "remote_server_password"
#define CONFIG_REMOTE_HTTP_SERVER_TYPE "remote_server_http_server_type"

#define CONFIG_LAST_SITE "last_site"

#define CONFIG_LOCAL_DIRECTORY "local_directory"
#define CONFIG_REMOTE_DIRECTORY "remote_directory"
#define CONFIG_UPDATE_WARN_MISSING "warn_missing_installs"

#define HTTP_SERVER_APACHE "Apache"
#define HTTP_SERVER_MS_IIS "Microsoft IIS"
#define HTTP_SERVER_NGINX "Nginx"
#define HTTP_SERVER_NPX_SERVE "Serve"
#define HTTP_SERVER_RCLONE "RClone"
#define HTTP_SERVER_ARCHIVEORG "Archive.org"
#define HTTP_SERVER_MYRIENT "Myrient"

#define CONFIG_LANGUAGE "language"
#define MAX_EDIT_FILE_SIZE 32768

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[128];
    char http_server_type[24];
    ClientType type;
};

extern bool swap_xo;
extern std::vector<std::string> sites;
extern std::vector<std::string> http_servers;
extern std::vector<std::string> langs;
extern std::map<std::string, RemoteSettings> site_settings;
extern std::set<std::string> text_file_extensions;
extern char local_directory[MAX_PATH_LENGTH];
extern char remote_directory[MAX_PATH_LENGTH];
extern char app_ver[6];
extern char last_site[32];
extern char display_site[64];
extern char language[32];
extern RemoteSettings *remote_settings;
extern bool warn_missing_installs;
extern RemoteClient *remoteclient;
extern bool show_hidden_files;
extern int max_edit_file_size;

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SetClientType(RemoteSettings *settings);
}
#endif
