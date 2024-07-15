#include <imgui_vita.h>
#include <stdio.h>
#include <algorithm>
#include <set>
#include "windows.h"
#include "textures.h"
#include "fs.h"
#include "config.h"
#include "ime_dialog.h"
#include "gui.h"
#include "net.h"
#include "actions.h"
#include "util.h"
#include "lang.h"
#include "debugnet.h"

extern "C"
{
#include "inifile.h"
}

#define MAX_EDIT_FILE_SIZE 32768

static SceCtrlData pad_prev;
bool paused = false;
int view_mode;
static float scroll_direction = 0.0f;
static int selected_local_position = -1;
static int selected_remote_position = -1;

static ime_callback_t ime_callback = nullptr;
static ime_callback_t ime_after_update = nullptr;
static ime_callback_t ime_before_update = nullptr;
static ime_callback_t ime_cancelled = nullptr;
static std::vector<std::string> *ime_multi_field;
static char *ime_single_field;
static int ime_field_size;

float previous_up = 0.0f;
float previous_down = 0.0f;
uint64_t bytes_transfered = 0;
uint64_t bytes_to_download;
SceUInt64 prev_tick;
std::vector<DirEntry> local_files;
std::vector<DirEntry> remote_files;
std::set<DirEntry> multi_selected_local_files;
std::set<DirEntry> multi_selected_remote_files;
std::vector<DirEntry> local_paste_files;
std::vector<DirEntry> remote_paste_files;
ACTIONS paste_action;
DirEntry selected_local_file;
DirEntry selected_remote_file;
ACTIONS selected_action;
char status_message[1024];
char local_file_to_select[256];
char remote_file_to_select[256];
char local_filter[32];
char remote_filter[32];
// char editor_text[1024];
char dialog_editor_text[1024];
char activity_message[1024];
int selected_browser = 0;
int saved_selected_browser;
bool activity_inprogess = false;
bool stop_activity = false;
bool file_transfering = false;
bool set_focus_to_local = false;
bool set_focus_to_remote = false;
char extract_zip_folder[256];
char zip_file_path[384];
char zip_password[128];

// Editor variables
std::vector<std::string> edit_buffer;
bool editor_inprogress = false;
char edit_line[1024];
int edit_line_num = 0;
char label[256];
bool editor_modified = false;
char edit_file[256];
int edit_line_to_select = -1;
std::string copy_text;

bool dont_prompt_overwrite = false;
bool dont_prompt_overwrite_cb = false;
int confirm_transfer_state = -1;
int overwrite_type = OVERWRITE_PROMPT;

int confirm_state = CONFIRM_NONE;
char confirm_message[256];
ACTIONS action_to_take = ACTION_NONE;

namespace Windows
{
    void Init()
    {
        remoteclient = NULL;

        sprintf(local_file_to_select, "..");
        sprintf(remote_file_to_select, "..");
        sprintf(status_message, "");
        sprintf(local_filter, "");
        sprintf(remote_filter, "");
        dont_prompt_overwrite = false;
        confirm_transfer_state = -1;
        dont_prompt_overwrite_cb = false;
        overwrite_type = OVERWRITE_PROMPT;

        Actions::RefreshLocalFiles(false);
    }

    void HandleWindowInput()
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        SceCtrlData pad;

        sceCtrlPeekBufferPositive(0, &pad, 1);

        if ((pad_prev.buttons & SCE_CTRL_SQUARE) && !(pad.buttons & SCE_CTRL_SQUARE) && !paused)
        {
            if (selected_browser & LOCAL_BROWSER && strcmp(selected_local_file.name, "..") != 0)
            {
                auto search_item = multi_selected_local_files.find(selected_local_file);
                if (search_item != multi_selected_local_files.end())
                {
                    multi_selected_local_files.erase(search_item);
                }
                else
                {
                    multi_selected_local_files.insert(selected_local_file);
                }
            }
            if (selected_browser & REMOTE_BROWSER && strcmp(selected_remote_file.name, "..") != 0)
            {
                auto search_item = multi_selected_remote_files.find(selected_remote_file);
                if (search_item != multi_selected_remote_files.end())
                {
                    multi_selected_remote_files.erase(search_item);
                }
                else
                {
                    multi_selected_remote_files.insert(selected_remote_file);
                }
            }
        }

        if ((pad_prev.buttons & SCE_CTRL_RTRIGGER) && !(pad.buttons & SCE_CTRL_RTRIGGER) && !paused)
        {
            set_focus_to_remote = true;
        }

        if ((pad_prev.buttons & SCE_CTRL_LTRIGGER) && !(pad.buttons & SCE_CTRL_LTRIGGER) && !paused)
        {
            set_focus_to_local = true;
        }

        pad_prev = pad;
    }

    void SetModalMode(bool modal)
    {
        paused = modal;
    }

    std::string getUniqueZipFilename()
    {
        std::string zipfolder;
        std::string zipname;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        if (strncmp(files.begin()->directory, "ux0:", 4) != 0 &&
            strncmp(files.begin()->directory, "uma0:", 5) != 0)
        {
            zipfolder = "ux0:data/";
        }
        else
        {
            zipfolder = files.begin()->directory;
        }

        if (files.size() == 1)
        {
            zipname = files.begin()->name;
        }
        else if (strcmp(files.begin()->directory, "/") == 0)
        {
            zipname = "new_zip";
        }
        else
        {
            zipname = std::string(files.begin()->directory);
            zipname = zipname.substr(zipname.find_last_of("/") + 1);
        }

        std::string zip_path;
        zip_path = zipfolder + "/" + zipname;
        int i = 0;
        while (true)
        {
            std::string temp_path;
            i > 0 ? temp_path = zip_path + "(" + std::to_string(i) + ").zip" : temp_path = zip_path + ".zip";
            if (!FS::FileExists(temp_path))
                return temp_path;
            i++;
        }
    }

    std::string getExtractFolder()
    {
        std::string zipfolder;
        std::vector<DirEntry> files;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if (local_browser_selected)
        {
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_local_file);
        }
        else
        {
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);
        }

        if (strncmp(local_directory, "ux0:", 4) != 0 &&
            strncmp(local_directory, "uma0:", 5) != 0)
        {
            zipfolder = "ux0:data";
        }
        else if (files.size() > 1)
        {
            zipfolder = local_directory;
        }
        else
        {
            std::string filename = std::string(files.begin()->name);
            size_t dot_pos = filename.find_last_of(".");
            zipfolder = std::string(local_directory) + "/" + filename.substr(0, dot_pos);
        }
        return zipfolder;
    }

    void ConnectionPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        static char title[128];
        sprintf(title, "v%s ezRemote %s", app_ver, lang_strings[STR_CONNECTION_SETTINGS]);
        BeginGroupPanel(title, ImVec2(945, 100));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
        char id[256];
        std::string hidden_password = std::string("xxxxxxx");

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetItemDefaultFocus();
        }
        bool is_connected = (remoteclient != nullptr) ? remoteclient->IsConnected() : false;
        void *icon = is_connected ? (void *)disconnect_icon : (void *)connect_icon;
        sprintf(id, "###connectbutton");
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 2);
        if (ImGui::ImageButtonEx(ImGui::GetID(id), (void *)icon, ImVec2(25, 25), ImVec2(0, 0), ImVec2(1, 1), style->FramePadding, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
        {
            selected_action = is_connected ? ACTION_DISCONNECT : ACTION_CONNECT;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", is_connected ? lang_strings[STR_DISCONNECT] : lang_strings[STR_CONNECT]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        ImGui::SetNextItemWidth(70);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 2);
        if (ImGui::BeginCombo("##Site", display_site, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
        {
            static char site_id[64];
            for (int n = 0; n < sites.size(); n++)
            {
                const bool is_selected = strcmp(sites[n].c_str(), last_site) == 0;
                sprintf(site_id, "%s %d", lang_strings[STR_SITE], n + 1);
                if (ImGui::Selectable(site_id, is_selected))
                {
                    sprintf(last_site, "%s", sites[n].c_str());
                    sprintf(display_site, site_id);
                    remote_settings = &site_settings[sites[n]];
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        int width = 350;
        if (remote_settings->type == CLIENT_TYPE_NFS)
            width = 600;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        if (ImGui::Button(remote_settings->server, ImVec2(width, 0)))
        {
            ime_single_field = remote_settings->server;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_callback = SingleValueImeCallback;
            ime_after_update = AferServerChangeCallback;
            Dialog::initImeDialog(lang_strings[STR_SERVER], remote_settings->server, 255, SCE_IME_TYPE_DEFAULT, 0, 0);
            gui_mode = GUI_MODE_IME;
        }

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::BeginCombo("##HttpServer", remote_settings->http_server_type, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
            {
                for (int n = 0; n < http_servers.size(); n++)
                {
                    const bool is_selected = strcmp(http_servers[n].c_str(), remote_settings->http_server_type) == 0;
                    if (ImGui::Selectable(http_servers[n].c_str(), is_selected))
                    {
                        sprintf(remote_settings->http_server_type, "%s", http_servers[n].c_str());
                    }
                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        if (remote_settings->type != CLIENT_TYPE_NFS)
        {
            ImGui::SameLine();
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_USERNAME]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
            sprintf(id, "%s##username", remote_settings->username);
            if (ImGui::Button(id, ImVec2(95, 0)))
            {
                ime_single_field = remote_settings->username;
                ResetImeCallbacks();
                ime_field_size = 47;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_USERNAME], remote_settings->username, 47, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
            }
            ImGui::SameLine();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_PASSWORD]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
            if (strlen(remote_settings->password) == 0)
                hidden_password = "";
            sprintf(id, "%s##password", hidden_password.c_str());
            if (ImGui::Button(id, ImVec2(60, 0)))
            {
                ime_single_field = remote_settings->password;
                ResetImeCallbacks();
                ime_field_size = 31;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_PASSWORD], remote_settings->password, 31, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
            }
        }

        ImGui::PopStyleVar();
        EndGroupPanel();
    }

    void BrowserPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        selected_browser = 0;

        BeginGroupPanel(lang_strings[STR_LOCAL], ImVec2(452, 420));
        float posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        ImVec2 size = ImGui::CalcTextSize(local_directory);
        ImGui::SetCursorPosX(posX + 110);
        if (ImGui::Button(local_directory, ImVec2(250, 0)))
        {
            ime_single_field = local_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterLocalFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], local_directory, 256, SCE_IME_TYPE_DEFAULT, 0, 0);
            gui_mode = GUI_MODE_IME;
        }
        if (size.x > 275 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(local_directory);
            ImGui::EndTooltip();
        }
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + 110);
        ImGui::PushID("local_filter##remote");
        if (ImGui::Button(local_filter, ImVec2(250, 0)))
        {
            ime_single_field = local_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], local_filter, 31, SCE_IME_TYPE_DEFAULT, 0, 0);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::PopStyleVar();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 17);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::PushID("search##local");
        if (ImGui::ImageButton((void *)search_icon, ImVec2(25, 25)))
        {
            selected_action = ACTION_APPLY_LOCAL_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        ImGui::PushID("refresh##local");
        if (ImGui::ImageButton((void *)refresh_icon, ImVec2(25, 25)))
        {
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }
        ImGui::PopID();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        ImGui::BeginChild("Local##ChildWindow", ImVec2(452, 315));
        ImGui::Separator();
        ImGui::Columns(3, "Local##Columns", true);
        int i = 0;
        if (set_focus_to_local)
        {
            set_focus_to_local = false;
            ImGui::SetWindowFocus();
        }
        for (int j = 0; j < local_files.size(); j++)
        {
            DirEntry item = local_files[j];
            ImGui::SetColumnWidth(-1, 25);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 5);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
            if (item.isDir)
            {
                ImGui::Image((void *)folder_icon, ImVec2(20, 20));
            }
            else
            {
                ImGui::Image((void *)file_icon, ImVec2(20, 20));
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, 318);
            ImGui::PushID(i);
            auto search_item = multi_selected_local_files.find(item);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(452, 0)))
            {
                selected_local_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
                }
                else
                {
                    std::string filename = Util::ToLower(selected_local_file.name);
                    size_t dot_pos = filename.find_last_of(".");
                    if (dot_pos != std::string::npos)
                    {
                        std::string ext = filename.substr(dot_pos);
                        if (text_file_extensions.find(ext) != text_file_extensions.end())
                        {
                            selected_action = ACTION_LOCAL_EDIT;
                        }
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::IsItemFocused())
            {
                selected_local_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 310)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text(item.name);
                    ImGui::EndTooltip();
                }
                ImGuiIO &io = ImGui::GetIO();
                if (io.NavInputs[ImGuiNavInput_DpadUp] == 1.0f && previous_up != 1.0f && !paused)
                {
                    if (j == 0)
                    {
                        selected_local_position = local_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if (io.NavInputs[ImGuiNavInput_DpadDown] == 1.0f && previous_down != 1.0f && !paused)
                {
                    if (j == local_files.size()-1)
                    {
                        selected_local_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
                previous_up = io.NavInputs[ImGuiNavInput_DpadUp];
                previous_down = io.NavInputs[ImGuiNavInput_DpadDown];
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(local_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    sprintf(local_file_to_select, "");
                }
                if (selected_local_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_local_position = -1;
                }
                selected_browser |= LOCAL_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, 90);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text(item.display_size);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();
        ImGui::SameLine();

        BeginGroupPanel(lang_strings[STR_REMOTE], ImVec2(452, 420));
        posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        size = ImGui::CalcTextSize(remote_directory);
        ImGui::SetCursorPosX(posX + 110);
        if (ImGui::Button(remote_directory, ImVec2(250, 0)))
        {
            ime_single_field = remote_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterRemoteFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], remote_directory, 256, SCE_IME_TYPE_DEFAULT, 0, 0);
            gui_mode = GUI_MODE_IME;
        }
        if (size.x > 275 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(remote_directory);
            ImGui::EndTooltip();
        }
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + 110);
        ImGui::PushID("remote_filter##remote");
        if (ImGui::Button(remote_filter, ImVec2(250, 0)))
        {
            ime_single_field = remote_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], remote_filter, 31, SCE_IME_TYPE_DEFAULT, 0, 0);
            gui_mode = GUI_MODE_IME;
        };
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::PopStyleVar();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 17);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::PushID("search##remote");
        if (ImGui::ImageButton((void *)search_icon, ImVec2(25, 25)))
        {
            selected_action = ACTION_APPLY_REMOTE_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        ImGui::PushID("refresh##remote");
        if (ImGui::ImageButton((void *)refresh_icon, ImVec2(25, 25)))
        {
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text(lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        ImGui::BeginChild(ImGui::GetID("Remote##ChildWindow"), ImVec2(452, 315));
        if (set_focus_to_remote)
        {
            set_focus_to_remote = false;
            ImGui::SetWindowFocus();
        }
        ImGui::Separator();
        ImGui::Columns(3, "Remote##Columns", true);
        i = 99999;
        for (int j = 0; j < remote_files.size(); j++)
        {
            DirEntry item = remote_files[j];
            ImGui::SetColumnWidth(-1, 25);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 5);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
            if (item.isDir)
            {
                ImGui::Image((void *)folder_icon, ImVec2(20, 20));
            }
            else
            {
                ImGui::Image((void *)file_icon, ImVec2(20, 20));
            }
            ImGui::NextColumn();

            ImGui::SetColumnWidth(-1, 318);
            auto search_item = multi_selected_remote_files.find(item);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            ImGui::PushID(i);
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(452, 0)))
            {
                selected_remote_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
                }
                else
                {
                    std::string filename = Util::ToLower(selected_remote_file.name);
                    size_t dot_pos = filename.find_last_of(".");
                    if (dot_pos != std::string::npos)
                    {
                        std::string ext = filename.substr(dot_pos);
                        if (text_file_extensions.find(ext) != text_file_extensions.end())
                        {
                            selected_action = ACTION_REMOTE_EDIT;
                        }
                    }
                }
            }
            if (ImGui::IsItemFocused())
            {
                selected_remote_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 310)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text(item.name);
                    ImGui::EndTooltip();
                }
                ImGuiIO &io = ImGui::GetIO();
                if (io.NavInputs[ImGuiNavInput_DpadUp] == 1.0f && previous_up != 1.0f && !paused)
                {
                    if (j == 0)
                    {
                        selected_remote_position = remote_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if (io.NavInputs[ImGuiNavInput_DpadDown] == 1.0f && previous_down != 1.0f && !paused)
                {
                    if (j == remote_files.size()-1)
                    {
                        selected_remote_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
                previous_up = io.NavInputs[ImGuiNavInput_DpadUp];
                previous_down = io.NavInputs[ImGuiNavInput_DpadDown];
            }
            ImGui::PopID();
            if (ImGui::IsItemFocused())
            {
                selected_remote_file = item;
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(remote_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    sprintf(remote_file_to_select, "");
                }
                if (selected_remote_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_remote_position = -1;
                }
                selected_browser |= REMOTE_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, 90);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text(item.display_size);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();
    }

    void StatusPanel()
    {
        BeginGroupPanel(lang_strings[STR_MESSAGES], ImVec2(945, 100));
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::Dummy(ImVec2(925, 30));
        ImGui::SetCursorPos(pos);
        ImGui::PushTextWrapPos(925);
        if (strncmp(status_message, "4", 1) == 0 || strncmp(status_message, "3", 1) == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), status_message);
        }
        else
        {
            ImGui::Text(status_message);
        }
        ImGui::PopTextWrapPos();
        ImGui::SameLine();
        EndGroupPanel();
    }

    int getSelectableFlag(uint32_t remote_action)
    {
        int flag = ImGuiSelectableFlags_Disabled;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if ((local_browser_selected && selected_local_file.selectable) ||
            (remote_browser_selected && selected_remote_file.selectable &&
             remoteclient != nullptr && (remoteclient->SupportedActions() & remote_action)))
        {
            flag = ImGuiSelectableFlags_None;
        }
        return flag;
    }

    void ShowActionsDialog()
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        int flags;

        if (io.NavInputs[ImGuiNavInput_Input] == 1.0f)
        {
            if (!paused)
                saved_selected_browser = selected_browser;

            if (saved_selected_browser > 0)
            {
                SetModalMode(true);
                ImGui::OpenPopup(lang_strings[STR_ACTIONS]);
            }
        }

        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;
        if (local_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(120, 130));
        }
        else if (remote_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(600, 130));
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(230, 150), ImVec2(230, 400), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_ACTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushID("Select All##settings");
            if (ImGui::Selectable(lang_strings[STR_SELECT_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_SELECT_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_SELECT_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Clear All##settings");
            if (ImGui::Selectable(lang_strings[STR_CLEAR_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_CLEAR_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_CLEAR_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            if (local_browser_selected)
            {
                ImGui::PushID("Cut##settings");
                if (ImGui::Selectable(lang_strings[STR_CUT], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    selected_action = ACTION_LOCAL_CUT;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                ImGui::PushID("Copy##settings");
                if (ImGui::Selectable(lang_strings[STR_COPY], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    selected_action = ACTION_LOCAL_COPY;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                ImGui::PushID("Paste##settings");
                flags = ImGuiSelectableFlags_Disabled;
                if (local_browser_selected && local_paste_files.size())
                    flags = ImGuiSelectableFlags_None;
                if (ImGui::Selectable(lang_strings[STR_PASTE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_LOCAL_PASTE;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                {
                    int height = local_browser_selected ? (local_paste_files.size() * 30) + 42 : (remote_paste_files.size() * 30) + 42;
                    ImGui::SetNextWindowSize(ImVec2(500, height));
                    ImGui::BeginTooltip();
                    int text_width = ImGui::CalcTextSize(lang_strings[STR_FILES]).x;
                    int file_pos = ImGui::GetCursorPosX() + text_width + 15;
                    ImGui::Text("%s: %s", lang_strings[STR_TYPE], paste_action == ACTION_LOCAL_CUT ? lang_strings[STR_CUT] : lang_strings[STR_COPY]);
                    ImGui::Text("%s:", lang_strings[STR_FILES]);
                    ImGui::SameLine();
                    std::vector<DirEntry> files = local_paste_files;
                    for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
                    {
                        ImGui::SetCursorPosX(file_pos);
                        ImGui::Text("%s", it->path);
                    }
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            ImGui::PushID("Delete##settings");
            if (ImGui::Selectable(lang_strings[STR_DELETE], false, getSelectableFlag(REMOTE_ACTION_DELETE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                confirm_state = CONFIRM_WAIT;
                sprintf(confirm_message, lang_strings[STR_DEL_CONFIRM_MSG]);
                if (local_browser_selected)
                    action_to_take = ACTION_DELETE_LOCAL;
                else if (remote_browser_selected)
                    action_to_take = ACTION_DELETE_REMOTE;
            }
            ImGui::PopID();
            ImGui::Separator();

            flags = getSelectableFlag(REMOTE_ACTION_RENAME);
            if ((local_browser_selected && multi_selected_local_files.size() > 1) ||
                (remote_browser_selected && multi_selected_remote_files.size() > 1))
                flags = ImGuiSelectableFlags_None;
            ImGui::PushID("Rename##settings");
            if (ImGui::Selectable(lang_strings[STR_RENAME], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_RENAME_LOCAL;
                else if (remote_browser_selected)
                    selected_action = ACTION_RENAME_REMOTE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FOLDER))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            ImGui::PushID("New Folder##settings");
            if (ImGui::Selectable(lang_strings[STR_NEW_FOLDER], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FOLDER;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FOLDER;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("New File##settings");
            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FILE))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_NEW_FILE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FILE;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FILE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            /*
            ImGui::PushID("Edit##settings");
            flags = ImGuiSelectableFlags_None;
            if ((remote_browser_selected && remoteclient != nullptr && (!(remoteclient->SupportedActions() & REMOTE_ACTION_EDIT) || selected_remote_file.isDir)) ||
                (local_browser_selected && selected_local_file.isDir))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_EDIT], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                {
                    selected_action = ACTION_LOCAL_EDIT;
                }
                else
                {
                    selected_action = ACTION_REMOTE_EDIT;
                }
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();
            */

            ImGui::PushID("Extract##settings");
            if (ImGui::Selectable(lang_strings[STR_EXTRACT], false, getSelectableFlag(REMOTE_ACTION_EXTRACT) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                ResetImeCallbacks();
                sprintf(extract_zip_folder, "%s", getExtractFolder().c_str());
                ime_single_field = extract_zip_folder;
                ime_field_size = 255;
                ime_callback = SingleValueImeCallback;
                if (local_browser_selected)
                    ime_after_update = AfterExtractFolderCallback;
                else
                    ime_after_update = AfterExtractRemoteFolderCallback;
                Dialog::initImeDialog(lang_strings[STR_EXTRACT_LOCATION], extract_zip_folder, 255, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
                file_transfering = false;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            flags = ImGuiSelectableFlags_Disabled;
            if (local_browser_selected)
            {
                ImGui::PushID("Compress##settings");
                if (ImGui::Selectable(lang_strings[STR_COMPRESS], false, getSelectableFlag(REMOTE_ACTION_NONE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    std::string zipname;
                    std::string zipfolder;

                    ResetImeCallbacks();
                    sprintf(zip_file_path, "%s", getUniqueZipFilename().c_str());
                    ime_single_field = zip_file_path;
                    ime_field_size = 383;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterZipFileCallback;
                    Dialog::initImeDialog(lang_strings[STR_ZIP_FILE_PATH], zip_file_path, 383, SCE_IME_TYPE_DEFAULT, 0, 0);
                    gui_mode = GUI_MODE_IME;
                    file_transfering = true;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                flags = getSelectableFlag(REMOTE_ACTION_UPLOAD);
                if (local_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_UPLOAD))
                {
                    flags = ImGuiSelectableFlags_Disabled;
                }
                ImGui::PushID("Upload##settings");
                if (ImGui::Selectable(lang_strings[STR_UPLOAD], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_UPLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            if (remote_browser_selected)
            {
                ImGui::PushID("Download##settings");
                if (ImGui::Selectable(lang_strings[STR_DOWNLOAD], false, getSelectableFlag(REMOTE_ACTION_DOWNLOAD) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_DOWNLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            ImGui::PushID("Install##settings");
            if (ImGui::Selectable(lang_strings[STR_INSTALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                selected_action = ACTION_INSTALL_LOCAL_PKG;
                if (remote_browser_selected)
                    selected_action = ACTION_INSTALL_REMOTE_PKG;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            flags = ImGuiSelectableFlags_Disabled;
            if (local_browser_selected || remote_browser_selected)
                flags = ImGuiSelectableFlags_None;
            ImGui::PushID("Properties##settings");
            if (ImGui::Selectable(lang_strings[STR_PROPERTIES], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_SHOW_LOCAL_PROPERTIES;
                else if (remote_browser_selected)
                    selected_action = ACTION_SHOW_REMOTE_PROPERTIES;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();

            ImGui::Separator();

            ImGui::PushID("Cancel##settings");
            if (ImGui::Selectable(lang_strings[STR_CANCEL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetItemDefaultFocus();
            }
            if (io.NavInputs[ImGuiNavInput_Cancel] == 1.0f)
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (confirm_state == CONFIRM_WAIT)
        {
            ImGui::OpenPopup(lang_strings[STR_CONFIRM]);
            ImGui::SetNextWindowPos(ImVec2(280, 200));
            ImGui::SetNextWindowSizeConstraints(ImVec2(430, 100), ImVec2(430, 200), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_CONFIRM], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 420);
                ImGui::Text(confirm_message);
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 150);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_NO], ImVec2(60, 0)))
                {
                    confirm_state = CONFIRM_NO;
                    selected_action = ACTION_NONE;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                };
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_YES], ImVec2(60, 0)))
                {
                    confirm_state = CONFIRM_YES;
                    selected_action = action_to_take;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        if (confirm_transfer_state == 0)
        {
            ImGui::OpenPopup(lang_strings[STR_OVERWRITE_OPTIONS]);
            ImGui::SetNextWindowPos(ImVec2(280, 180));
            ImGui::SetNextWindowSizeConstraints(ImVec2(420, 100), ImVec2(420, 400), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_OVERWRITE_OPTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::RadioButton(lang_strings[STR_DONT_OVERWRITE], &overwrite_type, 0);
                ImGui::RadioButton(lang_strings[STR_ASK_FOR_CONFIRM], &overwrite_type, 1);
                ImGui::RadioButton(lang_strings[STR_DONT_ASK_CONFIRM], &overwrite_type, 2);
                ImGui::Separator();
                ImGui::Checkbox("##AlwaysUseOption", &dont_prompt_overwrite_cb);
                ImGui::SameLine();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 380);
                ImGui::Text(lang_strings[STR_ALLWAYS_USE_OPTION]);
                ImGui::PopTextWrapPos();
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 110);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_CANCEL], ImVec2(100, 0)))
                {
                    confirm_transfer_state = 2;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    selected_action = ACTION_NONE;
                    ImGui::CloseCurrentPopup();
                };
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_CONTINUE], ImVec2(100, 0)))
                {
                    confirm_transfer_state = 1;
                    dont_prompt_overwrite = dont_prompt_overwrite_cb;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void ShowPropertiesDialog(DirEntry item)
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        SetModalMode(true);
        ImGui::OpenPopup(lang_strings[STR_PROPERTIES]);

        ImGui::SetNextWindowPos(ImVec2(240, 200));
        ImGui::SetNextWindowSizeConstraints(ImVec2(500, 80), ImVec2(500, 250), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_PROPERTIES], NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_TYPE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(105);
            ImGui::Text(item.isDir ? lang_strings[STR_FOLDER] : lang_strings[STR_FILE]);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_NAME]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(105);
            ImGui::TextWrapped(item.name);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_SIZE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(105);
            ImGui::Text("%lld   (%s)", item.file_size, item.display_size);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DATE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(105);
            ImGui::Text("%02d/%02d/%d %02d:%02d:%02d", item.modified.day, item.modified.month, item.modified.year,
                        item.modified.hours, item.modified.minutes, item.modified.seconds);
            ImGui::Separator();

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 200);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            if (ImGui::Button(lang_strings[STR_CLOSE], ImVec2(100, 0)))
            {
                SetModalMode(false);
                selected_action = ACTION_NONE;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void ShowProgressDialog()
    {
        if (activity_inprogess)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_PROGRESS]);

            ImGui::SetNextWindowPos(ImVec2(240, 200));
            ImGui::SetNextWindowSizeConstraints(ImVec2(480, 80), ImVec2(480, 200), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_PROGRESS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImVec2 cur_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(cur_pos);
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 430);
                ImGui::Text("%s", activity_message);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(cur_pos.y + 60);

                if (file_transfering)
                {
                    static float progress = 0.0f;
                    static double transfer_speed = 0.0f;
                    static char progress_text[32];
                    static SceUInt64 cur_tick;
                    static double tick_delta;

                    cur_tick = sceKernelGetProcessTimeWide();
                    tick_delta = (cur_tick - prev_tick) * 1.0f / 1000000.0f;

                    progress = (float)bytes_transfered / (float)bytes_to_download;
                    transfer_speed = (bytes_transfered * 1.0f / tick_delta) / 1048576.0f;

                    sprintf(progress_text, "%.3f MB/s", transfer_speed);
                    ImGui::ProgressBar(progress, ImVec2(465, 0), progress_text);
                }

                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 205);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_CANCEL]))
                {
                    stop_activity = true;
                    SetModalMode(false);
                }
                if (stop_activity)
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 460);
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), lang_strings[STR_CANCEL_ACTION_MSG]);
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndPopup();
                sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
            }
        }
    }

    void MainWindow()
    {
        Windows::SetupWindow();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        if (ImGui::Begin("Remote Client", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar))
        {
            ConnectionPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
            BrowserPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
            StatusPanel();
            ShowProgressDialog();
            ShowActionsDialog();
        }
        ImGui::End();
    }

    void ExecuteActions()
    {
        switch (selected_action)
        {
        case ACTION_CHANGE_LOCAL_DIRECTORY:
            Actions::HandleChangeLocalDirectory(selected_local_file);
            break;
        case ACTION_CHANGE_REMOTE_DIRECTORY:
            Actions::HandleChangeRemoteDirectory(selected_remote_file);
            break;
        case ACTION_REFRESH_LOCAL_FILES:
            Actions::HandleRefreshLocalFiles();
            break;
        case ACTION_REFRESH_REMOTE_FILES:
            Actions::HandleRefreshRemoteFiles();
            break;
        case ACTION_APPLY_LOCAL_FILTER:
            Actions::RefreshLocalFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_APPLY_REMOTE_FILTER:
            Actions::RefreshRemoteFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_NEW_LOCAL_FOLDER:
        case ACTION_NEW_REMOTE_FOLDER:
        case ACTION_NEW_LOCAL_FILE:
        case ACTION_NEW_REMOTE_FILE:
            if (gui_mode != GUI_MODE_IME)
            {
                sprintf(dialog_editor_text, "");
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog((selected_action == ACTION_NEW_LOCAL_FILE || selected_action == ACTION_NEW_REMOTE_FILE) ? lang_strings[STR_NEW_FILE] : lang_strings[STR_NEW_FOLDER], dialog_editor_text, 128, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_DELETE_LOCAL:
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedLocalFiles();
            break;
        case ACTION_DELETE_REMOTE:
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedRemotesFiles();
            break;
        case ACTION_UPLOAD:
            sprintf(status_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                Actions::UploadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_DOWNLOAD:
            sprintf(status_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                Actions::DownloadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_EXTRACT_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractLocalZips();
            break;
        case ACTION_EXTRACT_REMOTE_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractRemoteZips();
            break;
        case ACTION_CREATE_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::MakeLocalZip();
            break;
        case ACTION_RENAME_LOCAL:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_local_files.size() > 0)
                    sprintf(dialog_editor_text, "%s", multi_selected_local_files.begin()->name);
                else
                    sprintf(dialog_editor_text, "%s", selected_local_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_RENAME_REMOTE:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_remote_files.size() > 0)
                    sprintf(dialog_editor_text, "%s", multi_selected_remote_files.begin()->name);
                else
                    sprintf(dialog_editor_text, "%s", selected_remote_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, SCE_IME_TYPE_DEFAULT, 0, 0);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_SHOW_LOCAL_PROPERTIES:
            ShowPropertiesDialog(selected_local_file);
            break;
        case ACTION_SHOW_REMOTE_PROPERTIES:
            ShowPropertiesDialog(selected_remote_file);
            break;
        case ACTION_LOCAL_SELECT_ALL:
            Actions::SelectAllLocalFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_SELECT_ALL:
            Actions::SelectAllRemoteFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_CLEAR_ALL:
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CLEAR_ALL:
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_CONNECT:
            sprintf(status_message, "%s", "");
            Actions::Connect();
            break;
        case ACTION_DISCONNECT:
            sprintf(status_message, "%s", "");
            Actions::Disconnect();
            break;
        case ACTION_DISCONNECT_AND_EXIT:
            done = true;
            break;
        case ACTION_INSTALL_REMOTE_PKG:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            file_transfering = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            Actions::InstallRemotePkgs();
            selected_action = ACTION_NONE;
            break;
        case ACTION_INSTALL_LOCAL_PKG:
            activity_inprogess = true;
            file_transfering = true;
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            Actions::InstallLocalPkgs();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_CUT:
        case ACTION_LOCAL_COPY:
            paste_action = selected_action;
            local_paste_files.clear();
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(local_paste_files));
            else
                local_paste_files.push_back(selected_local_file);
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CUT:
        case ACTION_REMOTE_COPY:
            paste_action = selected_action;
            remote_paste_files.clear();
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(remote_paste_files));
            else
                remote_paste_files.push_back(selected_remote_file);
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_PASTE:
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_LOCAL_CUT)
                    Actions::MoveLocalFiles();
                else if (paste_action == ACTION_LOCAL_COPY)
                    Actions::CopyLocalFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_REMOTE_PASTE:
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_REMOTE_CUT)
                    Actions::MoveRemoteFiles();
                else if (paste_action == ACTION_REMOTE_COPY)
                    Actions::CopyRemoteFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_LOCAL_EDIT:
            if (selected_local_file.file_size > max_edit_file_size)
                sprintf(status_message, "%s %d", lang_strings[STR_MAX_EDIT_FILE_SIZE_MSG], max_edit_file_size);
            else
            {
                snprintf(edit_file, 255, "%s", selected_local_file.path);
                FS::LoadText(&edit_buffer, selected_local_file.path);
                editor_inprogress = true;
            }
            editor_modified = false;
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_EDIT:
            if (selected_remote_file.file_size > max_edit_file_size)
                sprintf(status_message, "%s %d", lang_strings[STR_MAX_EDIT_FILE_SIZE_MSG], max_edit_file_size);
            else if (remoteclient != nullptr && remoteclient->Get(TMP_EDITOR_FILE, selected_remote_file.path))
            {
                snprintf(edit_file, 255, "%s", selected_remote_file.path);
                FS::LoadText(&edit_buffer, TMP_EDITOR_FILE);
                editor_inprogress = true;
            }
            editor_modified = false;
            selected_action = ACTION_NONE;
            break;
        default:
            break;
        }
    }

    void ResetImeCallbacks()
    {
        ime_callback = nullptr;
        ime_after_update = nullptr;
        ime_before_update = nullptr;
        ime_cancelled = nullptr;
        ime_field_size = 1;
    }

    void HandleImeInput()
    {
        if (Dialog::isImeDialogRunning())
        {
            int ime_result = Dialog::updateImeDialog();
            if (ime_result == IME_DIALOG_RESULT_FINISHED || ime_result == IME_DIALOG_RESULT_CANCELED)
            {
                if (ime_result == IME_DIALOG_RESULT_FINISHED)
                {
                    if (ime_before_update != nullptr)
                    {
                        ime_before_update(ime_result);
                    }

                    if (ime_callback != nullptr)
                    {
                        ime_callback(ime_result);
                    }

                    if (ime_after_update != nullptr)
                    {
                        ime_after_update(ime_result);
                    }
                }
                else if (ime_cancelled != nullptr)
                {
                    ime_cancelled(ime_result);
                }

                ResetImeCallbacks();
                gui_mode = GUI_MODE_BROWSER;
            }
        }
    }

    void SingleValueImeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputTextUTF8();
            snprintf(ime_single_field, ime_field_size, "%s", new_value);
        }
    }

    void MultiValueImeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputTextUTF8();
            char *initial_value = (char *)Dialog::getImeDialogInitialText();
            if (strlen(initial_value) == 0)
            {
                ime_multi_field->push_back(std::string(new_value));
            }
            else
            {
                for (int i = 0; i < ime_multi_field->size(); i++)
                {
                    if (strcmp((*ime_multi_field)[i].c_str(), initial_value) == 0)
                    {
                        (*ime_multi_field)[i] = std::string(new_value);
                    }
                }
            }
        }
    }

    void NullAfterValueChangeCallback(int ime_result) {}

    void AfterLocalFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_LOCAL_FILES;
    }

    void AfterRemoteFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_REMOTE_FILES;
    }

    void AfterFolderNameCallback(int ime_result)
    {
        switch (selected_action)
        {
        case ACTION_NEW_LOCAL_FOLDER:
            Actions::CreateNewLocalFolder(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FOLDER:
            Actions::CreateNewRemoteFolder(dialog_editor_text);
            break;
        case ACTION_RENAME_LOCAL:
            if (multi_selected_local_files.size() > 0)
                Actions::RenameLocalFolder(multi_selected_local_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameLocalFolder(selected_local_file.path, dialog_editor_text);
            break;
        case ACTION_RENAME_REMOTE:
            if (multi_selected_remote_files.size() > 0)
                Actions::RenameRemoteFolder(multi_selected_remote_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameRemoteFolder(selected_remote_file.path, dialog_editor_text);
            break;
        case ACTION_NEW_LOCAL_FILE:
            Actions::CreateLocalFile(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FILE:
            Actions::CreateRemoteFile(dialog_editor_text);
            break;
        default:
            break;
        }
        selected_action = ACTION_NONE;
    }

    void CancelActionCallBack(int ime_result)
    {
        selected_action = ACTION_NONE;
    }

    void AfterExtractFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_LOCAL_ZIP;
    }

    void AfterExtractRemoteFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_REMOTE_ZIP;
    }

    void AfterZipFileCallback(int ime_result)
    {
        selected_action = ACTION_CREATE_LOCAL_ZIP;
    }

    void AferServerChangeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            CONFIG::SetClientType(remote_settings);
            if (strncasecmp(remote_settings->server, "https://archive.org/", 20) == 0)
            {
                sprintf(remote_settings->http_server_type, "%s", HTTP_SERVER_ARCHIVEORG);
            }
        }
    }

}
