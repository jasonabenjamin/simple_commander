/*
 * AI-Attribution License (AIAL) v2 — Draft
 */
#include "simple_commander.h"

static std::string fit_path_middle(const std::string& path, int max_width)
{
    if (max_width <= 0) return {};
    if (text_width(path) <= max_width) return path;

    const std::string ellipsis = "...";
    const int ellipsis_width = text_width(ellipsis);
    if (ellipsis_width >= max_width)
        return ellipsis;

    // Keep the beginning and the final component visible, like Windows
    // Explorer's abbreviated path display.
    std::size_t left_chars = path.size() / 2;
    std::size_t right_chars = path.size() - left_chars;
    while (left_chars > 0 && right_chars > 0)
    {
        std::string candidate = path.substr(0, left_chars) + ellipsis +
                                path.substr(path.size() - right_chars);
        if (text_width(candidate) <= max_width)
            return candidate;
        if (left_chars >= right_chars) --left_chars;
        else --right_chars;
    }
    return ellipsis;
}

static void draw_scrolled_path(SDL_Renderer* r, const std::string& path,
                               int x, int y, int width, SDL_Color color)
{
    if (width <= 0) return;

    // Keep the cursor/end of the edited path visible. The clipping rectangle
    // makes the path horizontally scroll inside the directory box instead of
    // drawing over the memory panel.
    std::string shown = path;
    if (text_width(shown) > width)
    {
        const std::string marker = "...";
        const int marker_width = text_width(marker);
        int available = std::max(1, width - marker_width);
        std::size_t start = 0;
        while (start < shown.size() && text_width(shown.substr(start)) > available)
            ++start;
        shown = marker + shown.substr(start);
    }

    SDL_Rect clip{x, y, width, 20};
    SDL_RenderSetClipRect(r, &clip);
    text(r, shown, x, y, color);
    SDL_RenderSetClipRect(r, nullptr);
}

void draw_screen(SDL_Renderer* r, int w, int h)
{
    SDL_SetRenderDrawColor(r, GREEN_BG.r, GREEN_BG.g, GREEN_BG.b, GREEN_BG.a);
    SDL_RenderClear(r);

    const int right = w - 8;
    // Give the command panel enough room for "Select All" and "Deselect"
    // while retaining two file columns at the minimum window size.
    const int command_left = w - 120;
    const int file_right = command_left - 7;
    const int top_h = 48;

    fill(r, 8, 8, 217, top_h, PANEL);
    fill(r, 232, 8, std::max(201, w - 439), top_h, PANEL);
    fill(r, std::max(440, w - 200), 8, 128, top_h, PANEL);
    fill(r, w - 64, 8, 56, top_h, PANEL);

    centered_text(r, "Volume: " + current_dir.root_name().string(), 8, 225, 16, CYAN);

    std::error_code ec;
    int count = 0;
    std::uintmax_t total = 0;
    if (special_view == SpecialView::FileView)
    {
        for (const auto& e : std::filesystem::directory_iterator(current_dir, ec))
        {
            if (ec) break;
            if (!show_hidden && is_hidden(e)) continue;
            ++count;
            if (!e.is_directory(ec)) total += e.file_size(ec);
        }
    }
    centered_text(r, std::to_string(count) + " Files", 8, 225, 31, CYAN);
    centered_text(r, format_size(total), 8, 225, 43, CYAN);

    text(r, "Simple Commander", 240, 14, CYAN);

    // Scroll the license horizontally inside the top-center panel.
    // The renderer is continuously refreshed by the main loop, so this
    // marquee remains animated even when there is no user input.
    const int license_left = 240;
    const int license_panel_right = 232 + std::max(201, w - 439) - 8;
    const int license_width = std::max(1, license_panel_right - license_left);
    const std::string license_text = "AI-Attribution License v2 (Draft)";
    const int license_text_width = text_width(license_text);

    if (license_text_width > license_width)
    {
        const int travel = license_text_width - license_width;
        const Uint32 pause_ms = 1200;
        const Uint32 scroll_ms = 3500;
        const Uint32 cycle_ms = pause_ms + scroll_ms + pause_ms + scroll_ms;
        const Uint32 t = SDL_GetTicks() % cycle_ms;
        int offset = 0;

        if (t < pause_ms)
        {
            offset = 0;
        }
        else if (t < pause_ms + scroll_ms)
        {
            const Uint32 elapsed = t - pause_ms;
            offset = static_cast<int>((static_cast<long long>(travel) * elapsed) / scroll_ms);
        }
        else if (t < pause_ms + scroll_ms + pause_ms)
        {
            offset = travel;
        }
        else
        {
            const Uint32 elapsed = t - pause_ms - scroll_ms - pause_ms;
            offset = travel - static_cast<int>((static_cast<long long>(travel) * elapsed) / scroll_ms);
        }

        SDL_Rect license_clip{license_left, 28, license_width, 18};
        SDL_RenderSetClipRect(r, &license_clip);
        text(r, license_text, license_left - offset, 28, CYAN);
        SDL_RenderSetClipRect(r, nullptr);
    }
    else
    {
        text(r, license_text, license_left, 28, CYAN);
    }

    text(r, "SDL2 File Manager", 240, 42, CYAN);

    const int dt_left = std::max(440, w - 200);
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm{};
    localtime_s(&now_tm, &now_t);
    std::ostringstream date;
    date << std::setfill('0') << std::setw(2) << now_tm.tm_mon + 1 << "-"
         << std::setw(2) << now_tm.tm_mday << "-" << std::setw(2) << now_tm.tm_year % 100;
    text(r, "Date:  " + date.str(), dt_left + 8, 16, CYAN);
    text(r, "Time:  " + current_time_24h(), dt_left + 8, 32, CYAN);

    text(r, "Page", w - 56, 16, CYAN);
    if (special_view == SpecialView::FileView)
        text(r, std::to_string(page + 1) + "/" + std::to_string(page_count(w, h)), w - 56, 32, CYAN);
    else
        text(r, "-", w - 40, 32, CYAN);

    const int mem_left = std::max(440, w - 200);
    const int path_right = mem_left - 7;
    fill(r, 8, 64, path_right - 8, 32, PANEL);
    std::string path_display;
    if (viewer_mode) path_display = viewer_path.string();
    else if (editor_mode) path_display = editor_path.string();
    else if (special_view == SpecialView::FileView) path_display = current_dir.string();
    else if (special_view == SpecialView::TreeView) path_display = tree_selected_path.string();
    const int path_text_width = std::max(1, path_right - 32);
    if (path_edit_mode)
    {
        fill(r, 12, 68, path_right - 16, 24, BLACK);
        draw_scrolled_path(r, path_edit_text + "_", 16, 72, path_text_width, CYAN);
    }
    else
    {
        const std::string shown_path = fit_path_middle(path_display, path_text_width);
        text(r, shown_path, 16, 72, CYAN);
    }

    if (path_error)
    {
        auto now = std::chrono::steady_clock::now();
        if (now < path_error_until)
        {
            fill(r, 12, 68, path_right - 16, 24, ERROR_RED);
            text(r, "No such directory", 16, 72, ERROR_YELLOW);
        }
        else path_error = false;
    }

    fill(r, mem_left, 64, right - mem_left, 32, PANEL);
    right_text(r, "Free Memory: " + startup_free_memory, right - 8, 68, CYAN);
    right_text(r, "Free on " + current_dir.root_name().string() + ": " + startup_free_drive, right - 8, 84, CYAN);

    const int main_top = 104;
    const int bottom_h = 32;
    const int main_bottom = h - bottom_h;
    const int main_h = std::max(36, main_bottom - main_top);

    fill(r, 8, main_top, file_right - 8, main_h, BLACK);
    if (!viewer_mode && !editor_mode)
        fill(r, command_left, main_top, right - command_left, main_h, PANEL);

    if (viewer_mode)
    {
        draw_viewer(r, w, file_right, main_top, main_bottom);
    }
    else if (editor_mode)
    {
        draw_editor(r, w, main_top, main_bottom);
    }
    else if (special_view == SpecialView::TreeView)
    {
        draw_tree(r, w, h, file_right, main_top, main_bottom);
    }
    else if (special_view == SpecialView::DrivesView)
    {
        draw_drives(r, file_right, main_top, main_bottom);
    }
    else
    {
        const int rows = visible_rows(h);
        const int cols = visible_columns(w);
        const int cell_w = std::max(175, (file_right - 16) / cols);
        const int name_width = std::max(100, cell_w - 48);
        const int first_y = main_top + 14;
        const int row_h = 18;
        const int cap = rows * cols;

        normalize_selection(w, h);
        const int logical_start = page * cap;
        for (int logical_offset = 0; logical_offset < cap; ++logical_offset)
        {
            const int logical = logical_start + logical_offset;
            if (logical >= logical_count()) break;
            const int col = logical_offset / rows;
            const int row = logical_offset % rows;
            const int x = 20 + col * cell_w;
            const int y = first_y + row * row_h;
            draw_file_entry(r, logical, x, y, name_width, logical == selected);
        }
    }

    if (!viewer_mode && !editor_mode)
    {
    const std::vector<std::string> commands = {
        "Drives", "Tree", "View", "Edit", "Mkdir", "Copy", "Move",
        "Delete", "Rename", "Select All", "Deselect", "Help"
    };
    int command_y = main_top + 12;
    for (int command_index = 0; command_index < (int)commands.size(); ++command_index)
    {
        const auto& command = commands[command_index];
        if (command_y + 12 >= main_bottom - 25) break;
        const bool active =
            (command_index == 5 && pending_operation == FileOperation::Copy) ||
            (command_index == 6 && pending_operation == FileOperation::Move) ||
            (command_index == 2 && view_select_mode) ||
            (command_index == 3 && edit_select_mode) ||
            (command_index == 7 && delete_mode);
        if (active)
        {
            fill(r, command_left + 4, command_y - 2, right - command_left - 8, 16, ERROR_RED);
            text(r, command, command_left + 8, command_y, YELLOW);
        }
        else
            text(r, command, command_left + 8, command_y, CYAN);
        command_y += 16;
    }

    const int arrow_y = main_bottom - 15;
    fill(r, command_left, arrow_y - 16, right - command_left, 1, ORANGE);
    up_arrow(r, command_left + 20, arrow_y, CYAN);
    down_arrow(r, right - 20, arrow_y, CYAN);
    }

    fill(r, 8, h - bottom_h, file_right - 8, bottom_h, GREEN_BG);
    fill(r, 16, h - bottom_h + 8, file_right - 24, bottom_h - 16, PANEL);
    std::string info;
    if (editor_mode)
    {
        info = editor_status.empty() ? (editor_dirty ? "Modified" : "Read OK") : editor_status;
        info += "  " + editor_path.filename().string();
        info += "  Ln " + std::to_string(editor_line + 1);
        info += "  Col " + std::to_string(editor_column + 1);
        info += "  " + std::to_string(editor_lines.size()) + " lines";
    }
    else if (viewer_mode)
    {
        info = viewer_path.filename().string();
        // Show the range of lines currently visible, not just the first line
        // on the page. This makes the status unambiguous when a page is full.
        const int viewer_rows = std::max(1, (main_bottom - (main_top + 14) - 4) / 18);
        const int first_line = std::min(viewer_scroll + 1, std::max(1, (int)viewer_lines.size()));
        const int last_line = std::min((int)viewer_lines.size(), viewer_scroll + viewer_rows);
        info += "  Lines " + std::to_string(first_line) + "-" +
                std::to_string(last_line) + "/" + std::to_string(viewer_lines.size());
    }
    else if (special_view == SpecialView::FileView)
    {
        if (has_parent_dir() && selected == 0)
            info = "..   <DIR>";
        else
        {
            const int index = selected - (has_parent_dir() ? 1 : 0);
            if (index >= 0 && index < (int)entries.size())
            {
                const auto& e = entries[index];
                info = e.path().filename().string();
                if (!e.is_directory())
                {
                    std::error_code sec;
                    info += "  " + format_size(e.file_size(sec));
                }
                info += "  " + date_time(e);
                info += "  " + file_attributes(e);
            }
        }
    }
    const int info_width = std::max(0, file_right - 24);
    info = fit_text_to_width(info, info_width);
    text(r, info, 16, h - 25, CYAN);

    if (pending_operation != FileOperation::None && !operation_sources.empty())
    {
        const int box_left = 16;
        const int box_right = std::max(box_left + 220, file_right - 8);
        fill(r, box_left, h - bottom_h + 8, box_right - box_left, bottom_h - 16, PANEL);
        std::string prompt = operation_label();
        if (operation_sources.size() == 1)
            prompt += "  " + operation_sources.front().filename().string() + "  To  ";
        else
            prompt += "  " + std::to_string(operation_sources.size()) + " Tagged Files  To  ";
        text(r, prompt + path_edit_text + "_", box_left + 8, h - 25, CYAN);
    }

    if (mkdir_mode)
    {
        const int box_left = 16;
        const int box_width = std::max(240, file_right - 24);
        const int label_width = text_width("Make Directory: ");
        const int visible_width = std::max(20, box_width - label_width - 20);
        fill(r, box_left, h - bottom_h + 8, box_width, bottom_h - 16, PANEL);
        text(r, "Make Directory: ", box_left + 8, h - 25, CYAN);
        std::string shown = mkdir_text;
        while (text_width(shown) > visible_width && !shown.empty()) shown.erase(0, 1);
        text(r, shown + "_", box_left + 8 + label_width, h - 25, YELLOW);
    }

    if (rename_mode && !rename_text.empty())
    {
        const int rename_width = text_width("Rename: ") + text_width("________________") + 16;
        const int rename_left = std::max(120, right - rename_width);
        fill(r, rename_left, h - bottom_h, right - rename_left, bottom_h, PANEL);
        std::string shown = rename_text;
        while (text_width(shown) > text_width("________________") && !shown.empty())
            shown.erase(0, 1);
        text(r, "Rename: " + shown + "_", rename_left + 8, h - 25, CYAN);
    }

    if (filter_mode)
    {
        const int filter_width = text_width("Filter: ") + text_width("________________") + 16;
        const int filter_left = std::max(120, right - filter_width);
        fill(r, filter_left, h - bottom_h, right - filter_left, bottom_h, PANEL);
        std::string shown = filter_text;
        while (text_width(shown) > text_width("________________") && !shown.empty())
            shown.pop_back();
        text(r, "Filter: " + shown + "_", filter_left + 8, h - 25, CYAN);
    }

    if (editor_save_prompt)
    {
        const int box_w = std::min(360, std::max(280, w - 80));
        const int box_h = 116;
        const int box_x = (w - box_w) / 2;
        const int box_y = main_top + (main_bottom - main_top - box_h) / 2;
        fill(r, box_x, box_y, box_w, box_h, ERROR_RED);
        fill(r, box_x + 8, box_y + 8, box_w - 16, box_h - 16, PANEL);
        centered_text(r, "FILE NOT SAVED", box_x + 8, box_x + box_w - 8, box_y + 16, YELLOW);
        const char* choices[] = {"Exit Without Saving", "Save and Exit", "Return to Editor"};
        const int option_y = box_y + 42;
        const int option_h = 20;
        for (int i = 0; i < 3; ++i)
        {
            if (i == editor_save_choice)
                fill(r, box_x + 8, option_y + i * option_h, box_w - 16, option_h, ERROR_RED);
            text(r, choices[i], box_x + 16, option_y + i * option_h,
                 i == editor_save_choice ? YELLOW : BLACK);
        }
    }

    if (delete_confirm_mode)
    {
        const std::string title_text = "Confirm Deleting " + delete_confirm_target.filename().string();
        const int title_width = text_width(title_text);
        const int choices_width = text_width("Yes") + 32;
        const int desired_width = std::max({360, title_width + 32, choices_width + 32});
        const int box_w = std::min(file_right - 16, desired_width);
        const int box_h = 96;
        const int box_x = 8 + (file_right - 8 - box_w) / 2;
        const int box_y = main_top + (main_h - box_h) / 2;
        fill(r, box_x, box_y, box_w, box_h, ERROR_RED);
        fill(r, box_x + 8, box_y + 8, box_w - 16, box_h - 16, PANEL);
        std::string title = fit_text_to_width(title_text, box_w - 24);
        text(r, title, box_x + 12, box_y + 16, YELLOW);
        const int option_y = box_y + 48;
        const int option_h = 22;
        fill(r, box_x + 8, option_y + delete_confirm_choice * option_h, box_w - 16, option_h, ERROR_RED);
        text(r, "No", box_x + 16, option_y, delete_confirm_choice == 0 ? WHITE : BLACK);
        text(r, "Yes", box_x + 16, option_y + option_h, delete_confirm_choice == 1 ? YELLOW : BLACK);
    }

}

