#include "fs.h"

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include "string.h"
#include "stdio.h"
#include "util.h"
#include "windows.h"
#include "lang.h"

#include <algorithm>

#define ERRNO_EEXIST (int)(0x80010000 + SCE_NET_EEXIST)
#define ERRNO_ENOENT (int)(0x80010000 + SCE_NET_ENOENT)

namespace FS
{
    int hasEndSlash(const char *path)
    {
        return path[strlen(path) - 1] == '/';
    }

    void MkDirs(const std::string &ppath, bool prev = false)
    {
        std::string path = ppath;
        if (!prev)
        {
            path.push_back('/');
        }
        auto ptr = path.begin();
        while (true)
        {
            ptr = std::find(ptr, path.end(), '/');
            if (ptr == path.end())
                break;

            char last = *ptr;
            *ptr = 0;
            int err = sceIoMkdir(path.c_str(), 0777);
            *ptr = last;
            ++ptr;
        }
    }

    void Rm(const std::string &file)
    {
        int err = sceIoRemove(file.c_str());
    }

    void RmDir(const std::string &path)
    {
        sceIoRmdir(path.c_str());
    }

    int64_t GetSize(const std::string &path)
    {
        SceIoStat stat;
        int err = sceIoGetstat(path.c_str(), &stat);
        if (err < 0)
        {
            return -1;
        }
        return stat.st_size;
    }

    bool FileExists(const std::string &path)
    {
        SceIoStat stat;
        return sceIoGetstat(path.c_str(), &stat) >= 0;
    }

    bool FolderExists(const std::string &path)
    {
        SceIoStat stat;
        sceIoGetstat(path.c_str(), &stat);
        return stat.st_mode & SCE_S_IFDIR;
    }

    void Rename(const std::string &from, const std::string &to)
    {
        // try to remove first because sceIoRename does not overwrite
        int res = sceIoRename(from.c_str(), to.c_str());
    }

    void *Create(const std::string &path)
    {
        SceUID fd = sceIoOpen(
            path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);

        return (void *)(intptr_t)fd;
    }

    void *OpenRW(const std::string &path)
    {
        SceUID fd = sceIoOpen(path.c_str(), SCE_O_RDWR, 0777);
        return (void *)(intptr_t)fd;
    }

    void *OpenRead(const std::string &path)
    {
        SceUID fd = sceIoOpen(path.c_str(), SCE_O_RDONLY, 0777);
        return (void *)(intptr_t)fd;
    }

    void *Append(const std::string &path)
    {
        SceUID fd =
            sceIoOpen(path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
        return (void *)(intptr_t)fd;
    }

    int64_t Seek(void *f, uint64_t offset)
    {
        auto const pos = sceIoLseek((intptr_t)f, offset, SCE_SEEK_SET);
        return pos;
    }

    int Read(void *f, void *buffer, uint32_t size)
    {
        const auto read = sceIoRead((SceUID)(intptr_t)f, buffer, size);
        return read;
    }

    int Write(void *f, const void *buffer, uint32_t size)
    {
        int write = sceIoWrite((SceUID)(intptr_t)f, buffer, size);
        return write;
    }

    void Close(void *f)
    {
        SceUID fd = (SceUID)(intptr_t)f;
        int err = sceIoClose(fd);
    }

    std::vector<char> Load(const std::string &path)
    {
        SceUID fd = sceIoOpen(path.c_str(), SCE_O_RDONLY, 0777);
        if (fd < 0)
            return std::vector<char>(0);

        const auto size = sceIoLseek(fd, 0, SCE_SEEK_END);
        sceIoLseek(fd, 0, SCE_SEEK_SET);

        std::vector<char> data(size);

        const auto read = sceIoRead(fd, data.data(), data.size());
        sceIoClose(fd);
        if (read < 0)
            return std::vector<char>(0);

        data.resize(read);

        return data;
    }

    bool LoadText(std::vector<std::string> *lines, const std::string &path)
    {
        FILE *fd = fopen(path.c_str(), "rb");
        if (fd == nullptr)
            return false;

        lines->clear();

        char buffer[1024];
        short bytes_read;
        std::vector<char> line = std::vector<char>(0);
        do
        {
            bytes_read = fread(buffer, sizeof(char), 1024, fd);
            if (bytes_read < 0)
            {
                fclose(fd);
                return false;
            }

            for (short i = 0; i < bytes_read; i++)
            {
                if (buffer[i] != '\r' && buffer[i] != '\n')
                {
                    line.push_back(buffer[i]);
                }
                else
                {
                    lines->push_back(std::string(line.data(), line.size()));
                    line = std::vector<char>(0);
                    if (buffer[i] == '\r' && buffer[i+1] == '\n')
                        i++;
                }
            }
        } while (bytes_read == 1024);
        if (line.size()>0)
            lines->push_back(std::string(line.data(), line.size()));
 
        fclose(fd);
        if (lines->size() == 0)
            lines->push_back("");
        return true;
    }

    bool SaveText(std::vector<std::string> *lines, const std::string &path)
    {
        FILE *fd = OpenRW(path);
        if (fd == nullptr)
            return false;

        char nl[1] = {'\n'};
        for (int i=0; i < lines->size(); i++)
        {
            Write(fd, lines->at(i).c_str(), lines->at(i).length());
            Write(fd, nl, 1);
        }

        fclose(fd);

        return true;
    }

    void Save(const std::string &path, const void *data, uint32_t size)
    {
        SceUID fd = sceIoOpen(
            path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (fd < 0)
            return;

        const char *data8 = static_cast<const char *>(data);
        while (size != 0)
        {
            int written = sceIoWrite(fd, data8, size);
            sceIoClose(fd);
            if (written <= 0)
                return;
            data8 += written;
            size -= written;
        }
    }

    std::vector<DirEntry> ListDir(const std::string &path, int *err)
    {
        std::vector<DirEntry> out;
        DirEntry entry;
        Util::SetupPreviousFolder(path, &entry);
        out.push_back(entry);

        const auto fd = sceIoDopen(path.c_str());
        *err = 0;
        if (static_cast<uint32_t>(fd) == 0x80010002 || fd < 0)
        {
            *err = 1;
            return out;
        }

        while (true)
        {
            SceIoDirent dirent;
            DirEntry entry;
            entry.selectable = true;
            const auto ret = sceIoDread(fd, &dirent);
            if (ret < 0)
            {
                *err = ret;
                sceIoDclose(fd);
                return out;
            }
            else if (ret == 0)
                break;
            else
            {
                snprintf(entry.directory, 512, "%s", path.c_str());
                snprintf(entry.name, 256, "%s", dirent.d_name);

                SceDateTime gmt;
                SceDateTime lt;
                gmt.day = dirent.d_stat.st_mtime.day;
                gmt.month = dirent.d_stat.st_mtime.month;
                gmt.year = dirent.d_stat.st_mtime.year;
                gmt.hour = dirent.d_stat.st_mtime.hour;
                gmt.minute = dirent.d_stat.st_mtime.minute;
                gmt.second = dirent.d_stat.st_mtime.second;
                gmt.microsecond = dirent.d_stat.st_mtime.microsecond;
                Util::convertUtcToLocalTime(&gmt, &lt);
                entry.modified.day = lt.day;
                entry.modified.month = lt.month;
                entry.modified.year = lt.year;
                entry.modified.hours = lt.hour;
                entry.modified.minutes = lt.minute;
                entry.modified.seconds = lt.second;
                entry.modified.microsecond = lt.microsecond;

                if (hasEndSlash(path.c_str()))
                {
                    sprintf(entry.path, "%s%s", path.c_str(), entry.name);
                }
                else
                {
                    sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
                }

                if (SCE_S_ISDIR(dirent.d_stat.st_mode))
                {
                    entry.isDir = true;
                    entry.file_size = 0;
                    sprintf(entry.display_size, lang_strings[STR_FOLDER]);
                }
                else
                {
                    entry.file_size = dirent.d_stat.st_size;
                    if (entry.file_size < 1024)
                    {
                        sprintf(entry.display_size, "%lldB", entry.file_size);
                    }
                    else if (entry.file_size < 1024 * 1024)
                    {
                        sprintf(entry.display_size, "%.2fKB", entry.file_size * 1.0f / 1024);
                    }
                    else if (entry.file_size < 1024 * 1024 * 1024)
                    {
                        sprintf(entry.display_size, "%.2fMB", entry.file_size * 1.0f / (1024 * 1024));
                    }
                    else
                    {
                        sprintf(entry.display_size, "%.2fGB", entry.file_size * 1.0f / (1024 * 1024 * 1024));
                    }
                    entry.isDir = false;
                }
                out.push_back(entry);
            }
        }
        sceIoDclose(fd);

        return out;
    }

    std::vector<std::string> ListFiles(const std::string &path)
    {
        const auto fd = sceIoDopen(path.c_str());
        if (static_cast<uint32_t>(fd) == 0x80010002)
            return std::vector<std::string>(0);
        if (fd < 0)
            return std::vector<std::string>(0);

        std::vector<std::string> out;
        while (true)
        {
            SceIoDirent dirent;
            const auto ret = sceIoDread(fd, &dirent);
            if (ret < 0)
            {
                sceIoDclose(fd);
                return out;
            }
            else if (ret == 0)
                break;

            if (SCE_S_ISDIR(dirent.d_stat.st_mode))
            {
                std::vector<std::string> files = FS::ListFiles(path + "/" + dirent.d_name);
                for (std::vector<std::string>::iterator it = files.begin(); it != files.end();)
                {
                    out.push_back(std::string(dirent.d_name) + "/" + *it);
                    ++it;
                }
            }
            else
            {
                out.push_back(dirent.d_name);
            }
        }
        sceIoDclose(fd);
        return out;
    }

    int RmRecursive(const std::string &path)
    {
        if (stop_activity)
            return 1;
        SceUID dfd = sceIoDopen(path.c_str());
        if (dfd >= 0)
        {
            int res = 0;

            do
            {
                SceIoDirent dir;
                memset(&dir, 0, sizeof(SceIoDirent));
                res = sceIoDread(dfd, &dir);
                if (res > 0)
                {
                    int path_length = strlen(path.c_str()) + strlen(dir.d_name) + 2;
                    char *new_path = malloc(path_length);
                    snprintf(new_path, path_length, "%s%s%s", path.c_str(), hasEndSlash(path.c_str()) ? "" : "/", dir.d_name);

                    if (SCE_S_ISDIR(dir.d_stat.st_mode))
                    {
                        int ret = RmRecursive(new_path);
                        if (ret <= 0)
                        {
                            sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], new_path);
                            free(new_path);
                            sceIoDclose(dfd);
                            return ret;
                        }
                    }
                    else
                    {
                        snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETING], new_path);
                        int ret = sceIoRemove(new_path);
                        if (ret < 0)
                        {
                            sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], new_path);
                            free(new_path);
                            sceIoDclose(dfd);
                            return ret;
                        }
                    }

                    free(new_path);
                }
            } while (res > 0 && !stop_activity);

            sceIoDclose(dfd);

            if (stop_activity)
                return 0;
            int ret = sceIoRmdir(path.c_str());
            if (ret < 0)
            {
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], path.c_str());
                return ret;
            }
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
        }
        else
        {
            int ret = sceIoRemove(path.c_str());
            if (ret < 0)
            {
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], path.c_str());
                return ret;
            }
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETED], path.c_str());
        }

        return 1;
    }

    std::string GetPath(const std::string &ppath1, const std::string &ppath2)
    {
        std::string path1 = ppath1;
        std::string path2 = ppath2;
        path1 = Util::Rtrim(path1, "/");
        path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
        return path1 + "/" + path2;
    }

    int Head(const std::string &path, void *buffer, uint16_t len)
    {
        FILE *file = OpenRead(path);
        if (file == nullptr)
            return 0;
        int ret = Read(file, buffer, len);
        if (ret != len)
        {
            Close(file);
            return 0;
        }
        Close(file);
        return 1;
    }

    bool Copy(const std::string &from, const std::string &to)
    {
        MkDirs(to, true);
        FILE *src = OpenRead(from);
        if (!src)
        {
            return false;
        }

        bytes_to_download = GetSize(from);

        FILE *dest = OpenRW(to);
        if (!dest)
        {
            Close(src);
            return false;
        }

        size_t bytes_read = 0;
        bytes_transfered = 0;
        const size_t buf_size = 0x10000;
        unsigned char *buf = new unsigned char[buf_size];

        do
        {
            bytes_read = Read(src, buf, buf_size);
            if (bytes_read < 0)
            {
                delete[] buf;
                Close(src);
                Close(dest);
                return false;
            }

            size_t bytes_written = Write(dest, buf, bytes_read);
            if (bytes_written != bytes_read)
            {
                delete[] buf;
                Close(src);
                Close(dest);
                return false;
            }

            bytes_transfered += bytes_read;
        } while (bytes_transfered < bytes_to_download);

        delete[] buf;
        Close(src);
        Close(dest);
        return true;
    }

    bool Move(const std::string &from, const std::string &to)
    {
        bool res = Copy(from, to);
        if (res)
            RmRecursive(from);
        else
            return res;

        return true;
    }
}
