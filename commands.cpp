/*
 * AI Freedom License (AIFL) 1.0
 */
#include "simple_commander.h"

void begin_mkdir()
{
    if (special_view != SpecialView::FileView) return;
    mkdir_mode = true;
    mkdir_text.clear();
    operation_error.clear();
    SDL_StartTextInput();
}

void finish_mkdir(int w, int h)
{
    if (mkdir_text.empty())
    {
        mkdir_mode = false;
        SDL_StopTextInput();
        return;
    }
    std::filesystem::path new_dir = current_dir / mkdir_text;
    std::error_code ec;
    const bool created = std::filesystem::create_directory(new_dir, ec);
    mkdir_mode = false;
    SDL_StopTextInput();
    if (!created || ec)
    {
        operation_error = "Mkdir failed";
        path_error = true;
        path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        return;
    }
    reload_directory();
    normalize_selection(w, h);
}

void activate_command(int command_index, int w, int h)
{
    switch (command_index)
    {
        case 0: // Drives
            enter_special_view(SpecialView::DrivesView);
            selected = 0;
            build_drives();
            break;
        case 1: // Tree
            if (special_view == SpecialView::TreeView)
            {
                special_view = SpecialView::FileView;
                selected = 0;
                page = 0;
                reload_directory();
            }
            else if (special_view != SpecialView::DrivesView)
            {
                enter_special_view(SpecialView::TreeView);
                build_tree();
            }
            break;
        case 2: // View
            if (special_view == SpecialView::FileView) begin_view();
            break;
        case 3: // Edit
            if (special_view == SpecialView::FileView) begin_edit();
            break;
        case 4: // Mkdir
            if (special_view == SpecialView::FileView) begin_mkdir();
            break;
        case 5: // Copy
            if (special_view == SpecialView::FileView) begin_copy_move(FileOperation::Copy);
            break;
        case 6: // Move
            if (special_view == SpecialView::FileView) begin_copy_move(FileOperation::Move);
            break;
        case 7: // Delete
            if (special_view == SpecialView::FileView) begin_delete_mode();
            break;
        case 8: // Rename
            if (special_view == SpecialView::FileView) begin_rename();
            break;
        case 9: // Select All
            if (special_view == SpecialView::FileView) select_all_files();
            break;
        case 10: // Deselect
            if (special_view == SpecialView::FileView) deselect_all_files();
            break;
        case 11: // Help
            begin_help();
            break;
        case 12: // Palette
            cycle_color_scheme();
            break;
    }
    (void)w;
    (void)h;
}

void open_selected(int w, int h)
{
    if (has_parent_dir() && selected == 0)
    {
        go_parent(w, h);
        return;
    }

    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index >= 0 && index < (int)entries.size())
    {
        std::error_code ec;
        const auto& entry = entries[index];
        if (entry.is_directory(ec))
        {
            current_dir = entry.path();
            selected = 0;
            page = 0;
            reload_directory();
            normalize_selection(w, h);
        }
        else if (!ec && is_executable(entry))
        {
            execute_path(entry.path());
        }
    }
}

