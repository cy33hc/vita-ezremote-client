#include "config.h"
#include "installer.h"
#include "fs.h"
#include "sfo.h"
#include "gui.h"
#include "zip_util.h"
#include "util.h"
#include "dbglogger.h"

extern "C"
{
#include "inifile.h"
}

char updater_message[256];

namespace Installer
{
    static void fpkg_hmac(const uint8_t *data, unsigned int len, uint8_t hmac[16])
    {
        SHA1_CTX ctx;
        uint8_t sha1[20];
        uint8_t buf[64];

        sha1_init(&ctx);
        sha1_update(&ctx, data, len);
        sha1_final(&ctx, sha1);

        memset(buf, 0, 64);
        memcpy(&buf[0], &sha1[4], 8);
        memcpy(&buf[8], &sha1[4], 8);
        memcpy(&buf[16], &sha1[12], 4);
        buf[20] = sha1[16];
        buf[21] = sha1[1];
        buf[22] = sha1[2];
        buf[23] = sha1[3];
        memcpy(&buf[24], &buf[16], 8);

        sha1_init(&ctx);
        sha1_update(&ctx, buf, 64);
        sha1_final(&ctx, sha1);
        memcpy(hmac, sha1, 16);
    }

    int MakeHeadBin(const std::string &path)
    {
        uint8_t hmac[16];
        uint32_t off;
        uint32_t len;
        uint32_t out;

        SceIoStat stat;
        memset(&stat, 0, sizeof(SceIoStat));

        std::string head_path = path + "/sce_sys/package/head.bin";
        std::string sfo_path = path + "/sce_sys/param.sfo";

        // Read param.sfo
        const auto sfo = FS::Load(sfo_path);

        // Get title id
        char titleid[12];
        memset(titleid, 0, sizeof(titleid));
        snprintf(titleid, 12, "%s", SFO::GetString(sfo.data(), sfo.size(), "TITLE_ID"));

        char title[128];
        memset(title, 0, sizeof(title));
        snprintf(title, 127, "%s", SFO::GetString(sfo.data(), sfo.size(), "TITLE"));
        sprintf(activity_message, "%s %s", lang_strings[STR_PROMOTING], title);

        // Enforce TITLE_ID format
        if (strlen(titleid) != 9)
            return -1;

        // Get content id
        char contentid[48];
        memset(contentid, 0, sizeof(contentid));
        snprintf(contentid, 48, "%s", SFO::GetString(sfo.data(), sfo.size(), "CONTENT_ID"));

        // Free sfo buffer
        sfo.clear();

        // Allocate head.bin buffer
        std::vector<char> head_bin_data = FS::Load(HEAD_BIN_PATH);
        uint8_t *head_bin = malloc(head_bin_data.size());
        memcpy(head_bin, head_bin_data.data(), head_bin_data.size());

        // Write full title id
        char full_title_id[48];
        snprintf(full_title_id, sizeof(full_title_id), "EP9000-%s_00-0000000000000000", titleid);
        strncpy((char *)&head_bin[0x30], strlen(contentid) > 0 ? contentid : full_title_id, 48);

        // hmac of pkg header
        len = ntohl(*(uint32_t *)&head_bin[0xD0]);
        fpkg_hmac(&head_bin[0], len, hmac);
        memcpy(&head_bin[len], hmac, 16);

        // hmac of pkg info
        off = ntohl(*(uint32_t *)&head_bin[0x8]);
        len = ntohl(*(uint32_t *)&head_bin[0x10]);
        out = ntohl(*(uint32_t *)&head_bin[0xD4]);
        fpkg_hmac(&head_bin[off], len - 64, hmac);
        memcpy(&head_bin[out], hmac, 16);

        // hmac of everything
        len = ntohl(*(uint32_t *)&head_bin[0xE8]);
        fpkg_hmac(&head_bin[0], len, hmac);
        memcpy(&head_bin[len], hmac, 16);

        // Make dir
        FS::MkDirs(head_path, true);

        // Write head.bin
        FS::Save(head_path, head_bin, head_bin_data.size());

        free(head_bin);

        return 0;
    }

    int CheckAppExist(const std::string &titleid)
    {
        int res;
        int ret;

        ret = scePromoterUtilityCheckExist(titleid.c_str(), &res);
        if (res < 0)
            return res;

        return ret >= 0;
    }

    int PromoteApp(const std::string &path)
    {
        int res;

        std::string sfo_path = path + "/sce_sys/param.sfo";
        const auto sfo = FS::Load(sfo_path);

        // Get title
        char title[256];
        memset(title, 0, sizeof(title));
        snprintf(title, 255, "%s", SFO::GetString(sfo.data(), sfo.size(), "TITLE"));
        sfo.clear();

        sprintf(activity_message, "%s %s", lang_strings[STR_PROMOTING], title);
        res = scePromoterUtilityPromotePkgWithRif(path.c_str(), 1);
        if (res < 0)
            return res;

        return res;
    }

    bool IsValidPackage(const DirEntry &entry, RemoteClient *client)
    {
        if (!entry.isDir)
        {
            std::vector<std::string> match_files = {"sce_sys/param.sfo"};
            if (ZipUtil::ContainsFiles(entry, match_files, client))
                return true;
        }
        else
        {
            if (client != nullptr)
            {
                return client->FileExists(std::string(entry.path) + "/sce_sys/param.sfo");
            }
            else
            {
                return FS::FileExists(std::string(entry.path) + "/sce_sys/param.sfo");
            }
        }
        return false;
    }

    std::string GetBasePackageDir(const std::string &path)
    {
        int err;
        std::string base_path = "";
        std::string sce_sys_path = path + "/sce_sys";
        if (FS::FolderExists(sce_sys_path))
            return path;

        std::vector<DirEntry> files = FS::ListDir(path, &err);
        for (int i = 0; i < files.size(); i++)
        {
            if (files[i].isDir && strcmp(files[i].name, "..") != 0)
            {
                base_path = GetBasePackageDir(files[i].path);
                if (base_path.length() > 0)
                    break;
            }
        }

        return base_path;
    }

    int InstallPackage(const DirEntry &entry, RemoteClient *client)
    {
        int res;
        std::string package_path;

        if (FS::FileExists(TMP_PACKAGE_DIR))
        {
            FS::RmRecursive(TMP_PACKAGE_DIR);
            FS::MkDirs(TMP_PACKAGE_DIR);
        }

        if (entry.isDir)
        {
            if (client != nullptr)
            {
                Actions::Download(entry, TMP_PACKAGE_DIR);
            }
        }
        else
        {
            ZipUtil::Extract(entry, TMP_PACKAGE_DIR, client);
        }

        // Make head.bin
        if (client != nullptr || !entry.isDir)
        {
            package_path = GetBasePackageDir(TMP_PACKAGE_DIR);
        }
        else
        {
            package_path = GetBasePackageDir(entry.path);
        }

        if (!FS::FileExists(package_path + "/sce_sys/package/head.bin"))
        {
            res = MakeHeadBin(package_path);
            if (res < 0)
                return res;
        }

        // Promote app
        res = PromoteApp(package_path);
        if (res < 0)
        {
            return res;
        }

        FS::RmRecursive(TMP_PACKAGE_DIR);

        return 0;
    }

}