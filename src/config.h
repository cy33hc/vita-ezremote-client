#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include "common.h"
#include "fs.h"
#include "clients/remote_client.h"

#define APP_ID "RMTCLI001"
#define DATA_PATH "ux0:data/" APP_ID
#define APP_PATH "ux0:app/" APP_ID
#define CONFIG_INI_FILE DATA_PATH "/config.ini"
#define TMP_EDITOR_FILE DATA_PATH "/tmp_editor.txt"

#define REMOTE_CLIENT_VPK_URL "https://github.com/cy33hc/vita-ezremote-client/releases/download/latest/ezremoteclient.vpk"
#define REMOTE_CLIENT_VERSION_URL "https://github.com/cy33hc/vita-ezremote-client/releases/download/latest/version.txt"
#define REMOTE_CLIENT_VERSION_PATH APP_PATH "/version.txt"
#define REMOTE_CLIENT_VERSION_UPDATE_PATH DATA_PATH "/tmp/version.txt"
#define REMOTE_CLIENT_VPK_UPDATE_PATH DATA_PATH "/tmp/webdavclient.vpk"

#define CONFIG_GLOBAL "Global"

#define CONFIG_SHOW_HIDDEN_FILES "show_hidden_files"
#define CONFIG_DEFAULT_STYLE_NAME "Default"
#define CONFIG_SWAP_XO "swap_xo"

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

#define CONFIG_LANGUAGE "language"

#define HTTP_SERVER_APACHE "Apache"
#define HTTP_SERVER_MS_IIS "Microsoft IIS"
#define HTTP_SERVER_NGINX "Nginx"
#define HTTP_SERVER_NPX_SERVE "Serve"

#define GOOGLE_OAUTH_HOST "https://oauth2.googleapis.com"
#define GOOGLE_AUTH_URL "https://oauth2.googleapis.com/device/code"
#define GOOGLE_API_URL "https://www.googleapis.com"
#define GOOGLE_DRIVE_API_PATH "/drive/v2/files"
#define GOOGLE_DRIVE_BASE_URL "https://drive.google.com"
#define GOOGLE_PERM_DRIVE "drive"
#define GOOGLE_PERM_DRIVE_APPDATA "drive.appdata"
#define GOOGLE_PERM_DRIVE_FILE "drive.file"
#define GOOGLE_PERM_DRIVE_METADATA "drive.metadata"
#define GOOGLE_PERM_DRIVE_METADATA_RO "drive.metadata.readonly"
#define GOOGLE_DEFAULT_PERMISSIONS GOOGLE_PERM_DRIVE
#define GOOGLE_SERVICE_ACCOUNT_PATH DATA_PATH "/google_serviceaccount.json"

struct GoogleAccountInfo
{
    char access_token[256];
    char refresh_token[256];
    uint64_t token_expiry;
};

struct GoogleAppInfo
{
    char client_id[140];
    char client_secret[64];
    char permissions[92];
};

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[128];
    ClientType type;
    char http_server_type[24];
    GoogleAccountInfo gg_account;
};

extern bool swap_xo;
extern std::vector<std::string> sites;
extern std::vector<std::string> http_servers;
extern std::map<std::string, RemoteSettings> site_settings;
extern char local_directory[MAX_PATH_LENGTH];
extern char remote_directory[MAX_PATH_LENGTH];
extern char app_ver[6];
extern char last_site[32];
extern char display_site[64];
extern char language[32];
extern RemoteSettings *remote_settings;
extern bool warn_missing_installs;
extern RemoteClient *remoteclient;
extern GoogleAppInfo gg_app;
extern bool show_hidden_files;

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SetClientType(RemoteSettings *settings);
}
#endif
