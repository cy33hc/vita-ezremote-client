#include <psp2/vshbridge.h>
#include "config.h"
#include "updater.h"
#include "sfo.h"
#include "fs.h"
#include "net.h"
#include "gui.h"

extern "C" {
	#include "inifile.h"
}

#define ITLS_ENSO_APP_ID "SKGTLSE12"

char updater_message[256];

namespace Updater {
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

    int MakeHeadBin()
    {
        uint8_t hmac[16];
        uint32_t off;
        uint32_t len;
        uint32_t out;

        SceIoStat stat;
        memset(&stat, 0, sizeof(SceIoStat));

        if (FS::FileExists(HEAD_BIN))
            return 0;

        // Read param.sfo
        const auto sfo = FS::Load(PACKAGE_DIR "/sce_sys/param.sfo");

        // Get title id
        char titleid[12];
        memset(titleid, 0, sizeof(titleid));
        snprintf(titleid, 12, "%s", SFO::GetString(sfo.data(), sfo.size(), "TITLE_ID"));

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
        fpkg_hmac(&head_bin[off], len-64, hmac);
        memcpy(&head_bin[out], hmac, 16);

        // hmac of everything
        len = ntohl(*(uint32_t *)&head_bin[0xE8]);
        fpkg_hmac(&head_bin[0], len, hmac);
        memcpy(&head_bin[len], hmac, 16);

        // Make dir
        sceIoMkdir(PACKAGE_DIR "/sce_sys/package", 0777);

        // Write head.bin
        FS::Save(HEAD_BIN, head_bin, head_bin_data.size());

        free(head_bin);

        return 0;
    }

    bool isiTLSEnsoInstalled()
    {
        return FS::FileExists("vs0:/data/external/itls/libhttp.suprx");
    }

    int CheckAppExist(const char *titleid)
    {
        int res;
        int ret;

        ret = scePromoterUtilityCheckExist(titleid, &res);
        if (res < 0)
            return res;

        return ret >= 0;
    }

    int PromoteApp(const char *path)
    {
        int res;

        sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

        res = scePromoterUtilityPromotePkgWithRif(path, 1);
        sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

        return res;
    }

    int InstallPackage(char *file, char *package_name)
    {
        int res;

        // Recursively clean up pkg directory
        sprintf(updater_message, "Removing temp pkg directory");
        FS::RmRecursive(PACKAGE_DIR);

        sprintf(updater_message, "Extracting vpk package");
        ExtractFile(file, PACKAGE_DIR "/", nullptr);

        // Make head.bin
        res = MakeHeadBin();
        if (res < 0)
            return res;

        // Promote app
        sprintf(updater_message, "Starting to install %s", package_name);
        res = PromoteApp(PACKAGE_DIR);
        if (res < 0)
        {
            sprintf(updater_message, "Failed to install %s", package_name);
            return res;
        }
        return 0;
    }

    void ExtractFile(char *file, char *dir, std::vector<std::string> *files_to_extract)
    {
        unz_global_info global_info;
        unz_file_info file_info;
        unzFile zipfile = unzOpen(file);
        unzGetGlobalInfo(zipfile, &global_info);
        unzGoToFirstFile(zipfile);
        uint64_t curr_extracted_bytes = 0;
        uint64_t curr_file_bytes = 0;
        int num_files = global_info.number_entry;
        char fname[512];
        char ext_fname[512];
        char read_buffer[8192];

        for (int zip_idx = 0; zip_idx < num_files; ++zip_idx)
        {
            unzGetCurrentFileInfo(zipfile, &file_info, fname, 512, NULL, 0, NULL, 0);
            sprintf(ext_fname, "%s%s", dir, fname); 
            const size_t filename_length = strlen(ext_fname);
            if (ext_fname[filename_length - 1] != '/' && ( files_to_extract == nullptr ||
                (files_to_extract != nullptr && std::find(files_to_extract->begin(), files_to_extract->end(), fname) != files_to_extract->end())))
            {
                snprintf(updater_message, 255, "Extracting %s", fname);
                curr_file_bytes = 0;
                unzOpenCurrentFile(zipfile);
                FS::MkDirs(ext_fname, true);
                FILE *f = fopen(ext_fname, "wb");
                while (curr_file_bytes < file_info.uncompressed_size)
                {
                    int rbytes = unzReadCurrentFile(zipfile, read_buffer, 8192);
                    if (rbytes > 0)
                    {
                        fwrite(read_buffer, 1, rbytes, f);
                        curr_extracted_bytes += rbytes;
                        curr_file_bytes += rbytes;
                    }
                }
                fclose(f);
                unzCloseCurrentFile(zipfile);
            }
            if ((zip_idx + 1) < num_files)
            {
                unzGoToNextFile(zipfile);
            }
        }
        unzClose(zipfile);
    }

    int UpdateWebdavClient()
    {
        std::vector<char> current_version;
        std::vector<char> update_version;
        char cur_ver[4];
        char upd_ver[4];
        int ret;

        FS::MkDirs(REMOTE_CLIENT_VPK_UPDATE_PATH, true);
        sprintf(updater_message, "Checking for Webdav Client Update");
        ret = Net::DownloadFile(REMOTE_CLIENT_VERSION_URL, REMOTE_CLIENT_VERSION_UPDATE_PATH);
        if (ret < 0)
        {
            sprintf(updater_message, "Failed to get updates");
            return -1;
        }

        current_version = FS::Load(REMOTE_CLIENT_VERSION_PATH);
        update_version = FS::Load(REMOTE_CLIENT_VERSION_UPDATE_PATH);
        snprintf(cur_ver, 4, "%s", current_version.data());
        snprintf(upd_ver, 4, "%s", update_version.data());
        ret = 0;
        if (strcmp(cur_ver, upd_ver) != 0)
        {
            sprintf(updater_message, "Downloading Webdav Client update");
            ret = Net::DownloadFile(REMOTE_CLIENT_VPK_URL, REMOTE_CLIENT_VPK_UPDATE_PATH);
            if (ret < 0)
            {
                sprintf(updater_message, "Failed to download update");
                return -1;
            }

            sprintf(updater_message, "Extracting files");
            ExtractFile(REMOTE_CLIENT_VPK_UPDATE_PATH, APP_PATH "/", nullptr);

            FS::Save(REMOTE_CLIENT_VERSION_PATH, upd_ver, 4);
            FS::Rm(REMOTE_CLIENT_VPK_UPDATE_PATH);
            FS::Rm(REMOTE_CLIENT_VERSION_UPDATE_PATH);

            return 1;
        }
        FS::Rm(REMOTE_CLIENT_VERSION_UPDATE_PATH);

        return 0;
    }

    void StartUpdaterThread()
    {
        updater_thid = sceKernelCreateThread("updater_thread", (SceKernelThreadEntry)UpdaterThread, 0x10000100, 0x4000, 0, 0, NULL);
		if (updater_thid >= 0)
			sceKernelStartThread(updater_thid, 0, NULL);
    }

    int UpdaterThread(SceSize args, void *argp)
    {
        SceKernelFwInfo fw;
        fw.size = sizeof(SceKernelFwInfo);
        _vshSblGetSystemSwVersion(&fw);
        int itls_enso_installed = isiTLSEnsoInstalled();
        int updated = 0;
        if (itls_enso_installed || fw.version > 0x03650000)
        {
            updated = UpdateWebdavClient();
        }

        if (!itls_enso_installed && fw.version <= 0x03650000 && warn_missing_installs)
        {
            sprintf(updater_message, "iTLS-Enso is not installed.\nIt's required to download updates");
            sceKernelDelayThread(5000000);
        }

        if (updated == 1)
        {
            sprintf(updater_message, "Webdav Client updated successfully.\nRestarting after 3s");
            sceKernelDelayThread(3000000);
            sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
        }

        // Just warn once only
        if (warn_missing_installs)
        {
            OpenIniFile(CONFIG_INI_FILE);
            WriteBool(CONFIG_GLOBAL, CONFIG_UPDATE_WARN_MISSING, false);
            WriteIniFile(CONFIG_INI_FILE);
            CloseIniFile();
        }

        handle_updates = false;
        Windows::SetModalMode(false);
        return sceKernelExitDeleteThread(0);
    }

    void StartInstallerThread()
    {
        int itls_enso_installed = CheckAppExist(ITLS_ENSO_APP_ID);

        if (!itls_enso_installed)
        {
            handle_updates = true;
            installer_thid = sceKernelCreateThread("installer_thread", (SceKernelThreadEntry)InstallerThread, 0x10000100, 0x4000, 0, 0, NULL);
            if (installer_thid >= 0)
                sceKernelStartThread(installer_thid, 0, NULL);
        }
    }

    int InstallerThread(SceSize args, void *argp)
    {
        sceKernelDelayThread(1500000);

        SceKernelFwInfo fw;
        fw.size = sizeof(SceKernelFwInfo);
        _vshSblGetSystemSwVersion(&fw);

        int itls_enso_installed = isiTLSEnsoInstalled();
        if (itls_enso_installed || fw.version > 0x03650000){}

    ERROR_EXIT:
        if (!itls_enso_installed && fw.version <= 0x03650000 && warn_missing_installs)
        {
            sprintf(updater_message, "iTLS-Enso is not installed.\nIt's required to download icons and updates");
            sceKernelDelayThread(4000000);
        }
        
        // Just warn once only
        if (warn_missing_installs)
        {
            OpenIniFile(CONFIG_INI_FILE);
            WriteBool(CONFIG_GLOBAL, CONFIG_UPDATE_WARN_MISSING, false);
            WriteIniFile(CONFIG_INI_FILE);
            CloseIniFile();
        }

        handle_updates = false;
        Windows::SetModalMode(false);
        return sceKernelExitDeleteThread(0);
    }
}