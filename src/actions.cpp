#include <vitasdk.h>
#include <string.h>
#include "clients/remote_client.h"
#include "clients/apache.h"
#include "clients/iis.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/smbclient.h"
#include "clients/ftpclient.h"
#include "clients/nfsclient.h"
#include "clients/webdavclient.h"
#include "config.h"
#include "common.h"
#include "lang.h"
#include "util.h"
#include "windows.h"
#include <debugnet.h>

namespace Actions
{
    static int FtpCallback(int64_t xfered, void* arg)
    {
        bytes_transfered = xfered;
        return 1;
    }

    void RefreshLocalFiles(bool apply_filter)
    {
        multi_selected_local_files.clear();
        local_files.clear();
        int err;
        if (strlen(local_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = FS::ListDir(local_directory, &err);
            std::string lower_filter = Util::ToLower(local_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    local_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            local_files = FS::ListDir(local_directory, &err);
        }
        DirEntry::Sort(local_files);
        if (err != 0)
            sprintf(status_message, "%s", lang_strings[STR_FAIL_READ_LOCAL_DIR_MSG]);
    }

    void RefreshRemoteFiles(bool apply_filter)
    {
        if (!client->Ping())
        {
            client->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        multi_selected_remote_files.clear();
        remote_files.clear();
        if (strlen(remote_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = client->ListDir(remote_directory);
            std::string lower_filter = Util::ToLower(remote_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    remote_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            remote_files = client->ListDir(remote_directory);
        }
        DirEntry::Sort(remote_files);
        sprintf(status_message, "%s", client->LastResponse());
    }

    void HandleChangeLocalDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            sprintf(local_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
            sprintf(local_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            sprintf(local_directory, "%s", entry.path);
        }
        RefreshLocalFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleChangeRemoteDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (!client->Ping())
        {
            client->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            if (temp_path.size() > 1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    sprintf(remote_directory, "/");
                }
                else
                {
                    sprintf(remote_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            sprintf(remote_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            sprintf(remote_directory, "%s", entry.path);
        }
        RefreshRemoteFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshLocalFiles()
    {
        int prev_count = local_files.size();
        RefreshLocalFiles(false);
        int new_count = local_files.size();
        if (prev_count != new_count)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshRemoteFiles()
    {
        int prev_count = remote_files.size();
        RefreshRemoteFiles(false);
        int new_count = remote_files.size();
        if (prev_count != new_count)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void CreateNewLocalFolder(char *new_folder)
    {
        sprintf(status_message, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = FS::GetPath(local_directory, folder);
        FS::MkDirs(path);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", folder.c_str());
    }

    void CreateNewRemoteFolder(char *new_folder)
    {
        sprintf(status_message, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = client->GetPath(remote_directory, folder);
        if (client->Mkdir(path.c_str()))
        {
            RefreshRemoteFiles(false);
            sprintf(remote_file_to_select, "%s", folder.c_str());
        }
        else
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], client->LastResponse());
        }
    }

    void RenameLocalFolder(char *old_path, char *new_path)
    {
        sprintf(status_message, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(local_directory, new_name);
        FS::Rename(old_path, path);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", new_name.c_str());
    }

    void RenameRemoteFolder(char *old_path, char *new_path)
    {
        sprintf(status_message, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(remote_directory, new_name);
        client->Rename(old_path, path.c_str());
        RefreshRemoteFiles(false);
        sprintf(remote_file_to_select, "%s", new_name.c_str());
    }

    int DeleteSelectedLocalFilesThread(SceSize args, void *argp)
    {
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            FS::RmRecursive(it->path);
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return sceKernelExitDeleteThread(0);
    }

    void DeleteSelectedLocalFiles()
    {
        sprintf(activity_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("delete_local_files_thread", (SceKernelThreadEntry)DeleteSelectedLocalFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int DeleteSelectedRemotesFilesThread(SceSize args, void *argp)
    {
        if (client->Ping())
        {
            std::vector<DirEntry> files;
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (it->isDir)
                    client->Rmdir(it->path, true);
                else
                {
                    sprintf(activity_message, "%s %s\n", lang_strings[STR_DELETING], it->path);
                    if (!client->Delete(it->path))
                    {
                        sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], client->LastResponse());
                    }
                }
            }
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        else
        {
            client->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            Disconnect();
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        return sceKernelExitDeleteThread(0);
    }

    void DeleteSelectedRemotesFiles()
    {
        sprintf(activity_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("delete_remote_files_thread", (SceKernelThreadEntry)DeleteSelectedRemotesFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int UploadFile(const char *src, const char *dest)
    {
        int ret;
        int64_t filesize;
        ret = client->Ping();
        if (ret == 0)
        {
            client->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return ret;
        }

        if (overwrite_type == OVERWRITE_PROMPT && client->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelDelayThread(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && client->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            sprintf(activity_message, "%s %s\n", lang_strings[STR_UPLOADING], src);
            return client->Put(src, dest);
        }

        return 1;
    }

    int Upload(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            client->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char*)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    client->Mkdir(new_path);
                    ret = Upload(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = UploadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char*)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], src.name);
            bytes_to_download = src.file_size;
            bytes_transfered = 0;
            ret = UploadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    int UploadFilesThread(SceSize args, void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
                Upload(*it, new_dir);
            }
            else
            {
                Upload(*it, remote_directory);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return sceKernelExitDeleteThread(0);
    }

    void UploadFiles()
    {
        sprintf(activity_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("upload_files_thread", (SceKernelThreadEntry)UploadFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int DownloadFile(const char *src, const char *dest)
    {
        int ret;
        bytes_transfered = 0;
        ret = client->Size(src, &bytes_to_download);
        debugNetPrintf(DEBUG, "bytes_to_download=%ld\n", bytes_to_download);
        if (ret == 0)
        {
            client->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return ret;
        }

        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelDelayThread(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            sprintf(activity_message, "%s %s\n", lang_strings[STR_DOWNLOADING], src);
            return client->Get(dest, src);
        }

        return 1;
    }

    int Download(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            debugNetPrintf(DEBUG, "Download - before ListDir\n");
            std::vector<DirEntry> entries = client->ListDir(src.path);
            debugNetPrintf(DEBUG, "Download - after ListDir\n");
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char*)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path);
                    ret = Download(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], entries[i].path);
                    ret = DownloadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char*)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], src.path);
            ret = DownloadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], src.path);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    int DownloadFilesThread(SceSize args, void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_remote_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                Download(*it, new_dir);
            }
            else
            {
                Download(*it, local_directory);
            }
        }
        file_transfering = false;
        activity_inprogess = false;
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return sceKernelExitDeleteThread(0);
    }

    void DownloadFiles()
    {
        sprintf(activity_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("download_files_thread", (SceKernelThreadEntry)DownloadFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int KeepAliveThread(SceSize args, void *argp)
    {
        SceUInt64 idle;
        while (true)
        {
            if (client != nullptr && client->clientType() == CLIENT_TYPE_FTP)
            {
                FtpClient *ftpclient = (FtpClient*)client;
                idle = ftpclient->GetIdleTime();
                if (idle > 60000000)
                {
                    if (!ftpclient->Noop())
                    {
                        ftpclient->Quit();
                        sprintf(status_message, lang_strings[STR_REMOTE_TERM_CONN_MSG]);
                        sceKernelExitDeleteThread(0);
                    }
                }
                sceKernelDelayThread(500000);
            }
            else
                sceKernelExitDeleteThread(0);
        }
        sceKernelExitDeleteThread(0);
    }

    void Connect()
    {
        CONFIG::SaveConfig();
        if (strncmp(remote_settings->server, "webdavs://", 10) == 0 || strncmp(remote_settings->server, "webdav://", 9) == 0)
        {
            client = new WebDAV::WebDavClient();
        }
        else if (strncmp(remote_settings->server, "smb://", 6) == 0)
        {
            client = new SmbClient();
        }
        else if (strncmp(remote_settings->server, "ftp://", 6) == 0)
        {
            FtpClient *ftpclient = new FtpClient();
            ftpclient->SetConnmode(FtpClient::pasv);
            ftpclient->SetCallbackBytes(256000);
            ftpclient->SetCallbackXferFunction(FtpCallback);
            client = ftpclient;
        }
        else if (strncmp(remote_settings->server, "nfs://", 6) == 0)
        {
            client = new NfsClient();
        }
        else if (strncmp(remote_settings->server, "http://", 7) == 0 || strncmp(remote_settings->server, "https://", 8) == 0)
        {
            client = new IISClient();
        }
        else
        {
            sprintf(status_message, "%s", lang_strings[STR_PROTOCOL_NOT_SUPPORTED]);
            selected_action = ACTION_NONE;
            return;
        }

        if (client->Connect(remote_settings->server, remote_settings->username, remote_settings->password))
        {
            RefreshRemoteFiles(false);
            sprintf(status_message, "%s", client->LastResponse());

            if (client->clientType() == CLIENT_TYPE_FTP)
            {
                ftp_keep_alive_thid = sceKernelCreateThread("ftp_keep_alive_thread", (SceKernelThreadEntry)KeepAliveThread, 0x10000100, 0x4000, 0, 0, NULL);
                if (ftp_keep_alive_thid >= 0)
                    sceKernelStartThread(ftp_keep_alive_thid, 0, NULL);
            }
        }
        else
        {
            sprintf(status_message, "%s", lang_strings[STR_FAIL_TIMEOUT_MSG]);
            if (client != nullptr)
            {
                client->Quit();
                delete client;
                client = nullptr;
            }
        }
        selected_action = ACTION_NONE;
    }

    void Disconnect()
    {
        if (client != nullptr)
        {
            client->Quit();
            multi_selected_remote_files.clear();
            remote_files.clear();
            sprintf(remote_directory, "/");
            sprintf(status_message, "");
        }
        client = nullptr;
    }

    void SelectAllLocalFiles()
    {
        for (int i = 0; i < local_files.size(); i++)
        {
            if (strcmp(local_files[i].name, "..") != 0)
                multi_selected_local_files.insert(local_files[i]);
        }
    }

    void SelectAllRemoteFiles()
    {
        for (int i = 0; i < remote_files.size(); i++)
        {
            if (strcmp(remote_files[i].name, "..") != 0)
                multi_selected_remote_files.insert(remote_files[i]);
        }
    }

    int CopyOrMoveLocalFile(const char *src, const char *dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelDelayThread(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            if (isCopy)
                return FS::Copy(src, dest);
            else
                return FS::Move(src, dest);
        }

        return 1;
    }

    int CopyOrMove(const DirEntry &src, const char *dest, bool isCopy)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path);
                    ret = CopyOrMove(entries[i], new_path, isCopy);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = CopyOrMoveLocalFile(entries[i].path, new_path, isCopy);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], src.name);
            bytes_to_download = src.file_size;
            ret = CopyOrMoveLocalFile(src.path, new_path, isCopy);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void MoveLocalFilesThread(SceSize args, void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                CopyOrMove(*it, new_dir, false);
                FS::RmRecursive(it->path);
            }
            else
            {
                CopyOrMove(*it, local_directory, false);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MoveLocalFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MoveLocalFilesThread, NULL);
        bk_activity_thid = sceKernelCreateThread("move_local_files", (SceKernelThreadEntry)MoveLocalFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
        else
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void CopyLocalFilesThread(SceSize args, void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                CopyOrMove(*it, new_dir, true);
            }
            else
            {
                CopyOrMove(*it, local_directory, true);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void CopyLocalFiles()
    {
        sprintf(status_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("copy_local_files", (SceKernelThreadEntry)CopyLocalFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
        else
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyOrMoveRemoteFile(const std::string &src, const std::string &dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && client->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest.c_str());
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelDelayThread(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && client->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            if (isCopy)
                return client->Copy(src, dest);
            else
                return client->Move(src, dest);
        }

        return 1;
    }

    void MoveRemoteFilesThread(SceSize args, void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
            }

            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_MOVING], it->path);
            int res = CopyOrMoveRemoteFile(it->path, new_path, false);
            if (res == 0)
                sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void MoveRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("move_remote_files", (SceKernelThreadEntry)MoveRemoteFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
        else
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyRemotePath(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = client->ListDir(src.path);
            client->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    client->Mkdir(new_path);
                    ret = CopyRemotePath(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = CopyOrMoveRemoteFile(entries[i].path, new_path, true);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_COPY_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], src.name);
            ret = CopyOrMoveRemoteFile(src.path, new_path, true);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_COPY_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void CopyRemoteFilesThread(SceSize args, void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                int res = CopyRemotePath(*it, new_path);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
            else
            {
                int res = CopyRemotePath(*it, remote_directory);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void CopyRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        bk_activity_thid = sceKernelCreateThread("copy_remote_files", (SceKernelThreadEntry)CopyRemoteFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
        if (bk_activity_thid >= 0)
            sceKernelStartThread(bk_activity_thid, 0, NULL);
        else
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void CreateLocalFile(char *filename)
    {
        std::string new_file = FS::GetPath(local_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!FS::FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }
        FILE* f = FS::Create(temp_file);
        FS::Close(f);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", temp_file.c_str());
    }

    void CreateRemoteFile(char *filename)
    {
        std::string new_file = FS::GetPath(remote_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!client->FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }

        SceRtcTick tick;
        sceRtcGetCurrentTick(&tick);
        std::string local_tmp = std::string(DATA_PATH) + "/" + std::to_string(tick.tick);
        FILE *f = FS::Create(local_tmp);
        FS::Close(f);
        client->Put(local_tmp, temp_file);
        FS::Rm(local_tmp);
        RefreshRemoteFiles(false);
        sprintf(remote_file_to_select, "%s", temp_file.c_str());
    }

}
