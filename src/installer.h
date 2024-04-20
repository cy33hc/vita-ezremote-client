#ifndef __INSTALLER_H__
#define __INSTALLER_H__
#include <vitasdk.h>
#include "clients/remote_client.h"
#include "config.h"
#include "windows.h"

extern "C" {
    #include "sha1.h"
}

#define ntohl __builtin_bswap32

#define HEAD_BIN_PATH APP_PATH "/head.bin"
#define PACKAGE_DIR DATA_PATH "/tmp/pkg"
#define HEAD_BIN PACKAGE_DIR "/sce_sys/package/head.bin"

namespace Installer {
    int PromoteApp(const std::string &path);
    int CheckAppExist(const std::string &titleid);
    int MakeHeadBin(const std::string &path);
    int InstallPackage(const DirEntry &entry, RemoteClient *client = nullptr);
    bool IsValidPackage(const DirEntry &entry, RemoteClient *client = nullptr);
}
#endif
