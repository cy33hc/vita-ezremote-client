#ifndef ACTIONS_H
#define ACTIONS_H

#include "common.h"

#define CONFIRM_NONE -1
#define CONFIRM_WAIT 0
#define CONFIRM_YES 1
#define CONFIRM_NO 2

enum ACTIONS {
    ACTION_NONE = 0,
    ACTION_UPLOAD,
    ACTION_DOWNLOAD,
    ACTION_DELETE_LOCAL,
    ACTION_DELETE_REMOTE,
    ACTION_RENAME_LOCAL,
    ACTION_RENAME_REMOTE,
    ACTION_NEW_LOCAL_FOLDER,
    ACTION_NEW_REMOTE_FOLDER,
    ACTION_CHANGE_LOCAL_DIRECTORY,
    ACTION_CHANGE_REMOTE_DIRECTORY,
    ACTION_APPLY_LOCAL_FILTER,
    ACTION_APPLY_REMOTE_FILTER,
    ACTION_REFRESH_LOCAL_FILES,
    ACTION_REFRESH_REMOTE_FILES,
    ACTION_SHOW_LOCAL_PROPERTIES,
    ACTION_SHOW_REMOTE_PROPERTIES,
    ACTION_LOCAL_SELECT_ALL,
    ACTION_REMOTE_SELECT_ALL,
    ACTION_LOCAL_CLEAR_ALL,
    ACTION_REMOTE_CLEAR_ALL,
    ACTION_CONNECT,
    ACTION_DISCONNECT,
    ACTION_DISCONNECT_AND_EXIT,
    ACTION_INSTALL_REMOTE_PKG,
    ACTION_INSTALL_LOCAL_PKG,
    ACTION_EXTRACT_LOCAL_ZIP,
    ACTION_CREATE_LOCAL_ZIP,
    ACTION_LOCAL_CUT,
    ACTION_LOCAL_COPY,
    ACTION_LOCAL_PASTE,
    ACTION_LOCAL_EDIT,
    ACTION_REMOTE_CUT,
    ACTION_REMOTE_COPY,
    ACTION_REMOTE_PASTE,
    ACTION_REMOTE_EDIT,
    ACTION_NEW_LOCAL_FILE,
    ACTION_NEW_REMOTE_FILE,
    ACTION_EXTRACT_REMOTE_ZIP,
};

enum OverWriteType {
    OVERWRITE_NONE = 0,
    OVERWRITE_PROMPT,
    OVERWRITE_ALL
};

static SceUID bk_activity_thid = -1;
static SceUID ftp_keep_alive_thid = -1;

namespace Actions {

    void RefreshLocalFiles(bool apply_filter);
    void RefreshRemoteFiles(bool apply_filter);
    void HandleChangeLocalDirectory(const DirEntry entry);
    void HandleChangeRemoteDirectory(const DirEntry entry);
    void HandleRefreshLocalFiles();
    void HandleRefreshRemoteFiles();
    void CreateNewLocalFolder(char *new_folder);
    void CreateNewRemoteFolder(char *new_folder);
    void RenameLocalFolder(char *old_path, char *new_path);
    void RenameRemoteFolder(char *old_path, char *new_path);
    int DeleteSelectedLocalFilesThread(SceSize args, void *argp);
    void DeleteSelectedLocalFiles();
    int DeleteSelectedRemotesFilesThread(SceSize args, void *argp);
    void DeleteSelectedRemotesFiles();
    int UploadFilesThread(SceSize args, void *argp);
    void UploadFiles();
    int  DownloadFilesThread(SceSize args, void *argp);
    int Download(const DirEntry &src, const char *dest);
    void DownloadFiles();
    void Connect();
    void Disconnect();
    void SelectAllLocalFiles();
    void SelectAllRemoteFiles();
    void ExtractZipThread(SceSize args, void *argp);
    void ExtractLocalZips();
    void ExtractRemoteZipThread(SceSize args, void *argp);
    void ExtractRemoteZips();
    void MakeZipThread(SceSize args, void *argp);
    void MakeLocalZip();
    void InstallRemotePkgsThread(SceSize args, void *argp);
    void InstallRemotePkgs();
    void InstallLocalPkgsThread(SceSize args, void *argp);
    void InstallLocalPkgs();
    void MoveLocalFilesThread(SceSize args, void *argp);
    void MoveLocalFiles();
    void CopyLocalFilesThread(SceSize args, void *argp);
    void CopyLocalFiles();
    void MoveRemoteFilesThread(SceSize args, void *argp);
    void MoveRemoteFiles();
    void CopyRemoteFilesThread(SceSize args, void *argp);
    void CopyRemoteFiles();
    void CreateLocalFile(char *filename);
    void CreateRemoteFile(char *filename);
}

#endif
