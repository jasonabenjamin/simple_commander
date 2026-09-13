/*
 * AI Freedom License (AIFL) 1.0
 */
#include "simple_commander.h"

std::vector<std::filesystem::path> operation_targets()
{
    std::vector<std::filesystem::path> result;
    if (special_view != SpecialView::FileView) return result;

    // A directory is always operated on individually under the cursor.
    // Files, on the other hand, may be operated on as a selected group.
    if (!(has_parent_dir() && selected == 0))
    {
        const int index = selected - (has_parent_dir() ? 1 : 0);
        if (index >= 0 && index < (int)entries.size())
        {
            std::error_code ec;
            if (entries[index].is_directory(ec) && !ec)
            {
                result.push_back(entries[index].path());
                return result;
            }
        }
    }

    if (!selected_files.empty())
    {
        for (const auto& p : selected_files)
        {
            std::error_code ec;
            if (std::filesystem::exists(p, ec) && !ec)
                result.push_back(p);
        }
        return result;
    }

    if (has_parent_dir() && selected == 0) return result;
    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index >= 0 && index < (int)entries.size())
        result.push_back(entries[index].path());
    return result;
}

std::filesystem::path cursor_target()
{
    if (special_view != SpecialView::FileView) return {};
    if (has_parent_dir() && selected == 0) return {};
    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index < 0 || index >= (int)entries.size()) return {};
    return entries[index].path();
}

std::string operation_label()
{
    if (pending_operation == FileOperation::Copy) return "Copy";
    if (pending_operation == FileOperation::Move) return "Move";
    return "";
}

void begin_rename()
{
    if (special_view != SpecialView::FileView) return;
    rename_mode = true;
    rename_text.clear();
    rename_scroll_px = 0;
    operation_error.clear();
    path_error = false;
    SDL_StopTextInput();
}

void finish_rename()
{
    const auto old_path = cursor_target();
    if (old_path.empty() || rename_text.empty() || rename_text == old_path.filename().string())
    {
        rename_mode = false;
        SDL_StopTextInput();
        return;
    }
    const auto new_path = old_path.parent_path() / rename_text;
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    rename_mode = false;
    SDL_StopTextInput();
    if (ec)
    {
        operation_error = "Rename failed";
        path_error = true;
        path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        return;
    }
    selected_files.erase(old_path);
    reload_directory();
}

void begin_delete_mode()
{
    if (special_view != SpecialView::FileView) return;
    if (cursor_target().empty()) return;
    delete_mode = true;
    delete_confirm_mode = false;
    delete_confirm_target.clear();
}

void refresh_after_operation()
{
    selected_files.clear();
    selected = 0;
    page = 0;
    reload_directory();
}

bool is_console_program(const std::filesystem::path& p)
{
    const std::string ext = lower(p.extension().string());
    if (ext == ".bat" || ext == ".cmd" || ext == ".com") return true;
    if (ext != ".exe") return false;

    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    IMAGE_DOS_HEADER dos{};
    in.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!in || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

    in.seekg(dos.e_lfanew, std::ios::beg);
    DWORD signature = 0;
    IMAGE_FILE_HEADER file_header{};
    WORD magic = 0;
    in.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    in.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!in || signature != IMAGE_NT_SIGNATURE) return false;

    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        IMAGE_OPTIONAL_HEADER32 opt{};
        opt.Magic = magic;
        in.read(reinterpret_cast<char*>(&opt) + sizeof(magic), sizeof(opt) - sizeof(magic));
        return in && opt.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI;
    }
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        IMAGE_OPTIONAL_HEADER64 opt{};
        opt.Magic = magic;
        in.read(reinterpret_cast<char*>(&opt) + sizeof(magic), sizeof(opt) - sizeof(magic));
        return in && opt.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI;
    }
    return false;
}

void execute_path(const std::filesystem::path& p)
{
    if (is_console_program(p))
    {
        // Run console programs in a normal Windows command window and pause
        // after exit so their output remains visible.
        std::wstring args = L"/C \"\"" + p.wstring() + L"\" & echo. & pause\"";
        ShellExecuteW(nullptr, L"open", L"cmd.exe", args.c_str(),
                      p.parent_path().wstring().c_str(), SW_SHOWNORMAL);
    }
    else
    {
        // GUI programs launch normally without an extra console or pause.
        ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr,
                      p.parent_path().wstring().c_str(), SW_SHOWNORMAL);
    }
}

void begin_copy_move(FileOperation op)
{
    pending_operation = op;
    operation_sources.clear();
    operation_error.clear();
    path_edit_text.clear();
    path_error = false;

    // Tagged files are already the source selection, so go directly to the
    // destination prompt. Otherwise the command enters an armed state and
    // the user clicks the file to choose the source.
    if (!selected_files.empty())
    {
        operation_sources = operation_targets();
        if (operation_sources.empty())
        {
            pending_operation = FileOperation::None;
            return;
        }
        path_edit_text = current_dir.string();
        SDL_StartTextInput();
    }
}

void finish_copy_move()
{
    std::filesystem::path destination(path_edit_text);
    std::error_code ec;
    if (path_edit_text.empty() || !std::filesystem::is_directory(destination, ec) || ec)
    {
        operation_error = "No such destination";
        path_error = true;
        path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        return;
    }

    destination = std::filesystem::absolute(destination, ec);
    if (ec) destination = std::filesystem::path(path_edit_text);

    bool failed = false;
    for (const auto& source : operation_sources)
    {
        std::error_code op_ec;
        const auto target = destination / source.filename();
        if (target == source) { failed = true; continue; }

        if (pending_operation == FileOperation::Copy)
        {
            if (std::filesystem::is_directory(source, op_ec))
                std::filesystem::copy(source, target, std::filesystem::copy_options::recursive, op_ec);
            else
                std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, op_ec);
        }
        else if (pending_operation == FileOperation::Move)
        {
            std::filesystem::rename(source, target, op_ec);
            if (op_ec)
            {
                op_ec.clear();
                if (std::filesystem::is_directory(source, op_ec))
                    std::filesystem::copy(source, target, std::filesystem::copy_options::recursive, op_ec);
                else
                    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, op_ec);
                if (!op_ec)
                    std::filesystem::remove_all(source, op_ec);
            }
        }
        if (op_ec) failed = true;
    }

    pending_operation = FileOperation::None;
    operation_sources.clear();
    SDL_StopTextInput();
    refresh_after_operation();
    if (failed)
    {
        operation_error = "Operation failed";
        path_error = true;
        path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }
}

void go_parent(int w, int h)
{
    // If this directory was entered directly from Tree, returning through the
    // parent entry should restore the Tree view rather than leaving it.
    if (special_view == SpecialView::FileView && !view_history.empty() &&
        view_history.back().view == SpecialView::TreeView &&
        view_history.back().tree_selected == current_dir)
    {
        previous_view(w, h);
        return;
    }

    auto p = current_dir.parent_path();
    if (p != current_dir)
    {
        current_dir = p;
        selected = 0;
        page = 0;
        reload_directory();
        normalize_selection(w, h);
    }
}

bool open_view_selected()
{
    if (special_view != SpecialView::FileView) return false;
    if (has_parent_dir() && selected == 0) return false;
    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index < 0 || index >= (int)entries.size()) return false;
    std::error_code ec;
    const auto& entry = entries[index];
    if (entry.is_directory(ec) || ec) return false;
    std::ifstream in(entry.path(), std::ios::binary);
    if (!in) return false;
    viewer_lines.clear();
    std::string line_text;
    while (std::getline(in, line_text))
    {
        if (!line_text.empty() && line_text.back() == '\r') line_text.pop_back();
        viewer_lines.push_back(line_text);
    }
    if (viewer_lines.empty()) viewer_lines.push_back("");
    viewer_path = entry.path();
    viewer_scroll = 0;
    viewer_mode = true;
    view_select_mode = false;
    SDL_StopTextInput();
    return true;
}

