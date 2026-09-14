/*
 * AI Freedom License (AIFL) 1.0
 */
#include "simple_commander.h"

static bool editor_ignore_opening_text = false;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    apply_color_scheme();
    // Allow the first mouse click to reach the SDL window even when Windows
    // is activating it with that click.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (TTF_Init() != 0 || !load_font())
    {
        std::fprintf(stderr, "SDL_ttf/font initialization failed.\n");
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Simple Commander",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        START_W, START_H,
        SDL_WINDOW_RESIZABLE);
    if (window)
        SDL_SetWindowMinimumSize(window, MIN_W, MIN_H);

    SDL_Renderer* renderer = window
        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
        : nullptr;

    if (!window || !renderer)
    {
        std::fprintf(stderr, "SDL creation failed: %s\n", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create a blank help file beside the executable if it is missing.
    {
        const auto help_path = help_file_path();
        std::error_code help_ec;

        if (!std::filesystem::exists(help_path, help_ec))
        {
            std::ofstream help_out(help_path, std::ios::binary);
        }
    }

    reload_directory();

    // Capture memory and drive space once at startup. These values do not
    // change while the program is running.
    startup_free_memory = free_memory_text();
    startup_free_drive = free_drive_text(current_dir);

    SDL_StartTextInput();

    bool running = true;
    bool close_requested = false;
    int w = START_W, h = START_H;

    while (running)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                if (editor_mode && editor_dirty)
                {
                    close_requested = true;
                    editor_save_prompt = true;
                    editor_save_choice = 2;
                }
                else
                {
                    running = false;
                }
            }

            else if (ev.type == SDL_WINDOWEVENT &&
                     ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                w = std::max(MIN_W, ev.window.data1);
                h = std::max(MIN_H, ev.window.data2);
                normalize_selection(w, h);
            }

            else if (ev.type == SDL_TEXTINPUT && editor_mode && !editor_save_prompt)
            {
                if (editor_ignore_opening_text)
                    editor_ignore_opening_text = false;
                else
                    insert_editor_text(ev.text.text);
            }

            else if (ev.type == SDL_TEXTINPUT && path_edit_mode)
            {
                path_edit_text += ev.text.text;
            }

            else if (ev.type == SDL_TEXTINPUT && rename_mode)
            {
                rename_text += ev.text.text;
            }

            else if (ev.type == SDL_TEXTINPUT && mkdir_mode)
            {
                mkdir_text += ev.text.text;
            }

            else if (ev.type == SDL_TEXTINPUT && pending_operation != FileOperation::None)
            {
                path_edit_text += ev.text.text;
            }

            else if (ev.type == SDL_TEXTINPUT && filter_mode)
            {
                // The '/' key can also generate an SDL_TEXTINPUT event. Ignore
                // that one event when it arrives after the key that opened the
                // filter, but do not discard the first character the user types.
                if (filter_ignore_slash && std::string(ev.text.text) == "/")
                {
                    filter_ignore_slash = false;
                    continue;
                }
                filter_ignore_slash = false;
                filter_text += ev.text.text;
                selected = 0;
                page = 0;
                reload_directory();
            }

            else if (ev.type == SDL_KEYDOWN)
            {
                const SDL_Keycode k = ev.key.keysym.sym;

                if (delete_confirm_mode)
                {
                    if (k == SDLK_LEFT || k == SDLK_UP) delete_confirm_choice = 0;
                    else if (k == SDLK_RIGHT || k == SDLK_DOWN) delete_confirm_choice = 1;
                    else if (k == SDLK_ESCAPE)
                    {
                        delete_confirm_mode = false;
                        delete_mode = false;
                        delete_confirm_target.clear();
                    }
                    else if (k == SDLK_RETURN)
                    {
                        if (delete_confirm_choice == 1)
                        {
                            std::error_code ec;
                            if (std::filesystem::is_directory(delete_confirm_target, ec))
                                std::filesystem::remove_all(delete_confirm_target, ec);
                            else
                                std::filesystem::remove(delete_confirm_target, ec);
                            if (ec)
                            {
                                operation_error = "Delete failed";
                                path_error = true;
                                path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                            }
                            else
                                selected_files.erase(delete_confirm_target);
                        }
                        delete_confirm_mode = false;
                        delete_mode = false;
                        delete_confirm_target.clear();
                        selected = std::max(0, selected - 1);
                        reload_directory();
                    }
                    continue;
                }

                if (editor_mode)
                {
                    if (editor_save_prompt)
                    {
                        handle_editor_prompt_key(k);

                        // If the prompt was opened by closing the window,
                        // keep the application open only when the user
                        // explicitly returns to the editor.
                        if (close_requested)
                        {
                            if (!editor_mode)
                            {
                                running = false;
                                close_requested = false;
                            }
                            else if (!editor_save_prompt)
                            {
                                close_requested = false;
                            }
                        }
                        continue;
                    }
                    auto& line_text = editor_lines[editor_line];
                    const SDL_Keymod mod = static_cast<SDL_Keymod>(ev.key.keysym.mod);
                    if ((mod & KMOD_ALT) && k == SDLK_d)
                    {
                        editor_delete_line();
                    }
                    else if ((mod & KMOD_ALT) && k == SDLK_i)
                    {
                        editor_insert_line();
                    }
                    else if ((mod & KMOD_ALT) && k == SDLK_w)
                    {
                        save_editor_file();
                    }
                    else if (k == SDLK_ESCAPE)
                    {
                        if (editor_dirty) { editor_save_prompt = true; editor_save_choice = 2; }
                        else finish_edit();
                    }
                    else if (k == SDLK_LEFT) editor_column = utf8_prev(line_text, editor_column);
                    else if (k == SDLK_RIGHT) editor_column = utf8_next(line_text, editor_column);
                    else if (k == SDLK_HOME) editor_column = 0;
                    else if (k == SDLK_END) editor_column = line_text.size();
                    else if (k == SDLK_UP)
                    {
                        if (editor_line > 0) --editor_line;
                        editor_column = std::min(editor_column, editor_lines[editor_line].size());
                    }
                    else if (k == SDLK_DOWN)
                    {
                        if (editor_line + 1 < (int)editor_lines.size()) ++editor_line;
                        editor_column = std::min(editor_column, editor_lines[editor_line].size());
                    }
                    else if (k == SDLK_PAGEUP)
                    {
                        editor_line = std::max(0, editor_line - std::max(1, (h - 104 - 32 - 18) / 18));
                        editor_column = std::min(editor_column, editor_lines[editor_line].size());
                    }
                    else if (k == SDLK_PAGEDOWN)
                    {
                        editor_line = std::min((int)editor_lines.size() - 1, editor_line + std::max(1, (h - 104 - 32 - 18) / 18));
                        editor_column = std::min(editor_column, editor_lines[editor_line].size());
                    }
                    else if (k == SDLK_BACKSPACE) editor_backspace();
                    else if (k == SDLK_DELETE) editor_delete();
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) editor_newline();
                    else if (k == SDLK_TAB) insert_editor_text("    ");
                    continue;
                }

                if (viewer_mode)
                {
                    if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_q) finish_view();
                    else if (k == SDLK_UP) viewer_scroll = std::max(0, viewer_scroll - 1);
                    else if (k == SDLK_DOWN)
                        viewer_scroll = std::min(std::max(0, viewer_visual_row_count(w) - 1), viewer_scroll + 1);
                    else if (k == SDLK_PAGEUP)
                        viewer_scroll = std::max(0, viewer_scroll - std::max(1, visible_rows(h) - 2));
                    else if (k == SDLK_PAGEDOWN)
                        viewer_scroll = std::min(std::max(0, viewer_visual_row_count(w) - 1),
                                                 viewer_scroll + std::max(1, visible_rows(h) - 2));
                    continue;
                }

                if (mkdir_mode)
                {
                    if (k == SDLK_ESCAPE) { mkdir_mode = false; mkdir_text.clear(); SDL_StopTextInput(); }
                    else if (k == SDLK_BACKSPACE) { if (!mkdir_text.empty()) mkdir_text.pop_back(); }
                    else if (k == SDLK_RETURN) finish_mkdir(w, h);
                    continue;
                }

                if (rename_mode)
                {
                    if (k == SDLK_ESCAPE)
                    {
                        rename_mode = false;
                        SDL_StopTextInput();
                    }
                    else if (k == SDLK_BACKSPACE)
                    {
                        if (!rename_text.empty()) rename_text.pop_back();
                    }
                    else if (k == SDLK_RETURN)
                        finish_rename();
                    continue;
                }

                if (view_select_mode || edit_select_mode)
                {
                    if (k == SDLK_ESCAPE)
                    {
                        view_select_mode = false;
                        edit_select_mode = false;
                    }
                    continue;
                }

                if (pending_operation != FileOperation::None)
                {
                    if (k == SDLK_ESCAPE)
                    {
                        pending_operation = FileOperation::None;
                        operation_sources.clear();
                        path_edit_text.clear();
                        SDL_StopTextInput();
                    }
                    else if (k == SDLK_BACKSPACE)
                    {
                        if (!path_edit_text.empty()) path_edit_text.pop_back();
                    }
                    else if (k == SDLK_RETURN)
                        finish_copy_move();
                    continue;
                }

                if (path_edit_mode)
                {
                    if (k == SDLK_ESCAPE)
                    {
                        path_edit_mode = false;
                        pending_operation = FileOperation::None;
                        operation_sources.clear();
                        SDL_StopTextInput();
                    }
                    else if (k == SDLK_BACKSPACE)
                    {
                        if (!path_edit_text.empty()) path_edit_text.pop_back();
                    }
                    else if (k == SDLK_RETURN)
                    {
                        std::filesystem::path target(path_edit_text);
                        std::error_code pec;
                        if (pending_operation != FileOperation::None)
                        {
                            finish_copy_move();
                        }
                        else if (!path_edit_text.empty() && std::filesystem::is_directory(target, pec) && !pec)
                        {
                            remember_view();
                            current_dir = std::filesystem::absolute(target, pec);
                            if (pec) current_dir = target;
                            special_view = SpecialView::FileView;
                            selected = 0; page = 0; reload_directory(); normalize_selection(w, h);
                            path_edit_mode = false;
                            SDL_StopTextInput();
                        }
                        else
                        {
                            path_error = true;
                            path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                        }
                    }
                    continue;
                }

                if (filter_mode)
                {
                    if (k == SDLK_ESCAPE || k == SDLK_RETURN)
                    {
                        filter_mode = false;
                        if (k == SDLK_ESCAPE) filter_text.clear();
                        reload_directory();
                    }
                    else if (k == SDLK_BACKSPACE)
                    {
                        if (!filter_text.empty()) filter_text.pop_back();
                        selected = 0;
                        page = 0;
                        reload_directory();
                    }
                    continue;
                }

                if (k == SDLK_ESCAPE) running = false;
                else if (k == SDLK_QUESTION || (k == SDLK_SLASH && (ev.key.keysym.mod & KMOD_SHIFT)))
                    activate_command(11, w, h);
                else if (k == SDLK_p)
                    activate_command(12, w, h);
                else if (k == SDLK_d)
                    activate_command(0, w, h);
                else if (k == SDLK_t)
                    activate_command(1, w, h);
                else if (k == SDLK_v)
                    open_view_selected();
                else if (k == SDLK_e)
                {
                    if (open_edit_selected())
                        editor_ignore_opening_text = true;
                }
                else if (k == SDLK_k)
                    activate_command(4, w, h);
                else if (k == SDLK_c)
                {
                    begin_copy_move(FileOperation::Copy);
                    if (pending_operation == FileOperation::Copy && operation_sources.empty())
                    {
                        operation_sources = operation_targets();
                        if (!operation_sources.empty())
                        {
                            path_edit_text = current_dir.string();
                            SDL_StartTextInput();
                        }
                    }
                }
                else if (k == SDLK_m)
                {
                    begin_copy_move(FileOperation::Move);
                    if (pending_operation == FileOperation::Move && operation_sources.empty())
                    {
                        operation_sources = operation_targets();
                        if (!operation_sources.empty())
                        {
                            path_edit_text = current_dir.string();
                            SDL_StartTextInput();
                        }
                    }
                }
                else if (k == SDLK_x)
                    activate_command(7, w, h);
                else if (k == SDLK_r)
                    activate_command(8, w, h);
                else if (k == SDLK_a)
                    activate_command(9, w, h);
                else if (k == SDLK_u)
                    activate_command(10, w, h);
                else if (k == SDLK_BACKSPACE && special_view != SpecialView::FileView)
                    previous_view(w, h);
                else if (special_view == SpecialView::TreeView && k == SDLK_UP)
                {
                    int idx = tree_index_of(tree_selected_path);
                    if (idx > 0) tree_selected_path = tree_rows[idx - 1].first;
                }
                else if (special_view == SpecialView::TreeView && k == SDLK_DOWN)
                {
                    int idx = tree_index_of(tree_selected_path);
                    if (idx + 1 < (int)tree_rows.size()) tree_selected_path = tree_rows[idx + 1].first;
                }
                else if (special_view == SpecialView::TreeView && k == SDLK_PAGEUP)
                {
                    const int rows = std::max(1, (h - 104 - 32 - 18) / 18);
                    tree_scroll = std::max(0, tree_scroll - rows);
                    if (tree_scroll < (int)tree_rows.size()) tree_selected_path = tree_rows[tree_scroll].first;
                }
                else if (special_view == SpecialView::TreeView && k == SDLK_PAGEDOWN)
                {
                    const int rows = std::max(1, (h - 104 - 32 - 18) / 18);
                    tree_scroll = std::min(std::max(0, (int)tree_rows.size() - rows), tree_scroll + rows);
                    if (tree_scroll < (int)tree_rows.size()) tree_selected_path = tree_rows[tree_scroll].first;
                }
                else if (special_view == SpecialView::TreeView && ((k == SDLK_EQUALS && (ev.key.keysym.mod & KMOD_SHIFT)) || k == SDLK_KP_PLUS))
                {
                    expand_tree_directory(tree_selected_path);
                }
                else if (special_view == SpecialView::TreeView && (k == SDLK_MINUS || k == SDLK_KP_MINUS))
                {
                    collapse_tree_directory(tree_selected_path);
                }
                else if (special_view == SpecialView::TreeView && k == SDLK_RETURN)
                {
                    remember_view();
                    current_dir = tree_selected_path;
                    special_view = SpecialView::FileView;
                    selected = 0; page = 0; reload_directory(); normalize_selection(w, h);
                }
                else if (special_view == SpecialView::DrivesView && k == SDLK_UP)
                {
                    if (selected > 0) --selected;
                }
                else if (special_view == SpecialView::DrivesView && k == SDLK_DOWN)
                {
                    if (selected + 1 < (int)drive_rows.size()) ++selected;
                }
                else if (special_view == SpecialView::DrivesView && k == SDLK_PAGEUP)
                {
                    const int rows = std::max(1, (h - 104 - 32 - 18) / 18);
                    selected = std::max(0, selected - rows);
                }
                else if (special_view == SpecialView::DrivesView && k == SDLK_PAGEDOWN)
                {
                    const int rows = std::max(1, (h - 104 - 32 - 18) / 18);
                    selected = std::min(std::max(0, (int)drive_rows.size() - 1), selected + rows);
                }
                else if (special_view == SpecialView::DrivesView && k == SDLK_RETURN)
                {
                    if (selected >= 0 && selected < (int)drive_rows.size())
                    {
                        const auto old_root = current_dir.root_path();
                        drive_current_dirs[old_root] = current_dir;
                        const auto target_root = drive_rows[selected].root_path();
                        auto it = drive_current_dirs.find(target_root);
                        const auto target_dir = (it != drive_current_dirs.end() && std::filesystem::is_directory(it->second))
                            ? it->second : drive_rows[selected];
                        remember_view();
                        current_dir = target_dir;
                        special_view = SpecialView::FileView;
                        selected = 0; page = 0; reload_directory(); normalize_selection(w, h);
                    }
                }
                else if (special_view != SpecialView::FileView)
                    continue;
                else if (k == SDLK_UP)
                {
                    // Move to the previous item in column-major order.
                    // At the top of the second (or later) column this moves
                    // to the bottom of the preceding column. At the top of
                    // the page, do not cross into the previous page.
                    const int cap = page_capacity(w, h);
                    const int page_start = page * cap;
                    if (selected > page_start)
                        --selected;
                    normalize_selection(w, h);
                }
                else if (k == SDLK_DOWN)
                {
                    if (selected + 1 < logical_count()) ++selected;
                    normalize_selection(w, h);
                }
                else if (k == SDLK_LEFT)
                {
                    const int rows = visible_rows(h);
                    if (selected >= rows) selected -= rows;
                    normalize_selection(w, h);
                }
                else if (k == SDLK_RIGHT)
                {
                    const int rows = visible_rows(h);
                    if (selected + rows < logical_count()) selected += rows;
                    normalize_selection(w, h);
                }
                else if (k == SDLK_PAGEUP)
                {
                    page = std::max(0, page - 1);
                    const int cap = page_capacity(w, h);
                    selected = std::min(std::max(0, logical_count() - 1), page * cap);
                }
                else if (k == SDLK_PAGEDOWN)
                {
                    const int pages = page_count(w, h);
                    page = std::min(pages - 1, page + 1);
                    const int cap = page_capacity(w, h);
                    selected = std::min(std::max(0, logical_count() - 1), page * cap);
                }
                else if (k == SDLK_BACKSPACE)
                {
                    if (!view_history.empty() && view_history.back().view != SpecialView::FileView)
                        previous_view(w, h);
                    else
                        go_parent(w, h);
                }
                else if (k == SDLK_j)
                    toggle_selected_file();
                else if (k == SDLK_h)
                {
                    show_hidden = !show_hidden;
                    selected = 0;
                    page = 0;
                    reload_directory();
                }
                else if (k == SDLK_RETURN)
                    open_selected(w, h);
                else if (k == SDLK_SLASH)
                {
                    filter_mode = true;
                    filter_ignore_slash = true;
                    filter_text.clear();
                    SDL_StartTextInput();
                }
            }

            else if (ev.type == SDL_MOUSEWHEEL && viewer_mode)
            {
                const int rows = std::max(1, (h - 104 - 18) / 18);
                const int max_scroll = std::max(0, viewer_visual_row_count(w) - rows);
                if (ev.wheel.y > 0)
                    viewer_scroll = std::max(0, viewer_scroll - 3);
                else if (ev.wheel.y < 0)
                    viewer_scroll = std::min(max_scroll, viewer_scroll + 3);
            }

            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_RIGHT)
            {
                if (special_view == SpecialView::TreeView)
                {
                    const int x = ev.button.x;
                    const int y = ev.button.y;
                    const int command_left = w - 120;
                    const int file_right = command_left - 7;
                    const int main_top = 104;
                    const int main_bottom = h - 32;
                    if (x >= 8 && x < file_right && y >= main_top && y < main_bottom)
                    {
                        const int max_rows = std::max(1, (main_bottom - main_top - 18) / 18);
                        const int idx = tree_scroll + (y - (main_top + 14)) / 18;
                        if (idx >= 0 && idx < (int)tree_rows.size() && idx < tree_scroll + max_rows)
                        {
                            tree_selected_path = tree_rows[idx].first;
                            std::error_code ec;
                            if (std::filesystem::is_directory(tree_selected_path, ec) && !ec)
                            {
                                if (tree_expanded.count(tree_selected_path))
                                    collapse_tree_directory(tree_selected_path);
                                else
                                    expand_tree_directory(tree_selected_path);
                            }
                        }
                    }
                }
            }

            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT)
            {
                const int x = ev.button.x;
                const int y = ev.button.y;
                const int command_left = w - 120;
                const int right = w - 8;
                const int mem_left = std::max(440, w - 200);
                const int path_right = mem_left - 7;
                const int main_top = 104;
                const int main_bottom = h - 32;
                const int arrow_y = main_bottom - 15;
                const int rows = visible_rows(h);
                const int cols = visible_columns(w);
                const int cap = rows * cols;
                const int file_right = command_left - 7;
                const int cell_w = std::max(175, (file_right - 16) / cols);
                const int command_y0 = main_top + 12;

                if (editor_save_prompt)
                {
                    const int box_w = std::min(360, std::max(280, w - 80));
                    const int box_h = 116;
                    const int box_x = (w - box_w) / 2;
                    const int box_y = main_top + (main_bottom - main_top - box_h) / 2;
                    const int option_y = box_y + 42;
                    const int option_h = 20;
                    if (x >= box_x + 8 && x < box_x + box_w - 8 &&
                        y >= option_y && y < option_y + option_h * 3)
                    {
                        const int choice = (y - option_y) / option_h;
                        editor_save_choice = choice;
                        if (choice == 0) finish_edit();
                        else if (choice == 1)
                        {
                            if (save_editor_file()) finish_edit();
                        }
                        else
                            editor_save_prompt = false;
                    }
                    continue;
                }

                if (delete_confirm_mode)
                {
                    const int box_w = std::min(360, std::max(260, file_right - 40));
                    const int box_h = 88;
                    const int box_x = 8 + (file_right - 8 - box_w) / 2;
                    const int box_y = main_top + (main_bottom - main_top - box_h) / 2;
                    const int option_y = box_y + 48;
                    const int option_h = 22;
                    if (x >= box_x + 8 && x < box_x + box_w - 8 &&
                        y >= option_y && y < option_y + option_h * 2)
                    {
                        const int choice = (y - option_y) / option_h;
                        if (choice == 1)
                        {
                            std::error_code ec;
                            if (std::filesystem::is_directory(delete_confirm_target, ec))
                                std::filesystem::remove_all(delete_confirm_target, ec);
                            else
                                std::filesystem::remove(delete_confirm_target, ec);
                            if (ec)
                            {
                                operation_error = "Delete failed";
                                path_error = true;
                                path_error_until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                            }
                            else
                                selected_files.erase(delete_confirm_target);
                        }
                        delete_confirm_mode = false;
                        delete_mode = false;
                        delete_confirm_target.clear();
                        selected = std::max(0, selected - 1);
                        reload_directory();
                    }
                    else if (x >= box_x && x < box_x + box_w && y >= box_y && y < box_y + box_h)
                    {
                        delete_confirm_choice = (y >= option_y + option_h) ? 1 : 0;
                    }
                    continue;
                }

                if (x >= 8 && x < path_right && y >= 64 && y < 96 && special_view != SpecialView::DrivesView)
                {
                    path_edit_mode = true;
                    path_edit_text = (special_view == SpecialView::TreeView) ? tree_selected_path.string() : current_dir.string();
                    path_error = false;
                    operation_error.clear();
                    SDL_StartTextInput();
                }
                else if (x >= command_left && x <= right && y >= command_y0 && y < command_y0 + 12 * 16)
                {
                    const int command_index = (y - command_y0) / 16;
                    activate_command(command_index, w, h);
                }
                else if (special_view == SpecialView::DrivesView && x >= 8 && x < file_right && y >= main_top && y < main_bottom)
                {
                    int idx = (y - (main_top + 14)) / 18;
                    if (idx >= 0 && idx < (int)drive_rows.size())
                    {
                        selected = idx;
                        if (ev.button.clicks == 2)
                        {
                            const auto old_root = current_dir.root_path();
                            drive_current_dirs[old_root] = current_dir;
                            const auto target_root = drive_rows[idx].root_path();
                            auto it = drive_current_dirs.find(target_root);
                            const auto target_dir = (it != drive_current_dirs.end() && std::filesystem::is_directory(it->second))
                                ? it->second : drive_rows[idx];
                            remember_view();
                            current_dir = target_dir;
                            special_view = SpecialView::FileView;
                            selected = 0; page = 0; reload_directory(); normalize_selection(w, h);
                        }
                    }
                }
                else if (special_view == SpecialView::TreeView && x >= 8 && x < file_right && y >= main_top && y < main_bottom)
                {
                    const int max_rows = std::max(1, (main_bottom - main_top - 18) / 18);
                    const int idx = tree_scroll + (y - (main_top + 14)) / 18;
                    if (idx >= 0 && idx < (int)tree_rows.size() && idx < tree_scroll + max_rows)
                    {
                        tree_selected_path = tree_rows[idx].first;
                        if (ev.button.clicks == 2)
                        {
                            remember_view();
                            current_dir = tree_selected_path;
                            special_view = SpecialView::FileView;
                            selected = 0; page = 0; reload_directory(); normalize_selection(w, h);
                        }
                    }
                }
                else if (x >= command_left && x <= right && y >= arrow_y - 16 && y <= arrow_y + 8)
                {
                    if (special_view == SpecialView::TreeView)
                    {
                        const int rows = std::max(1, (main_bottom - main_top - 18) / 18);
                        if (x < (command_left + right) / 2)
                        {
                            tree_scroll = std::max(0, tree_scroll - rows);
                            if (tree_scroll < (int)tree_rows.size()) tree_selected_path = tree_rows[tree_scroll].first;
                        }
                        else
                        {
                            tree_scroll = std::min(std::max(0, (int)tree_rows.size() - rows), tree_scroll + rows);
                            if (tree_scroll < (int)tree_rows.size()) tree_selected_path = tree_rows[tree_scroll].first;
                        }
                    }
                    else if (special_view == SpecialView::DrivesView)
                    {
                        const int rows = std::max(1, (main_bottom - main_top - 18) / 18);
                        if (x < (command_left + right) / 2) selected = std::max(0, selected - rows);
                        else selected = std::min(std::max(0, (int)drive_rows.size() - 1), selected + rows);
                    }
                    else
                    {
                        const int pages = page_count(w, h);
                        if (x < (command_left + right) / 2) page = std::max(0, page - 1);
                        else page = std::min(pages - 1, page + 1);
                        selected = std::min(std::max(0, logical_count() - 1), page * cap);
                    }
                }
                else if (x >= 8 && x < file_right && y >= main_top && y < main_bottom)
                {
                    const int col = std::clamp((x - 20) / cell_w, 0, cols - 1);
                    const int row = std::clamp((y - (main_top + 14)) / 18, 0, rows - 1);
                    const int logical = page * cap + col * rows + row;
                    if (logical >= 0 && logical < item_count())
                    {
                        selected = logical;
                        if (view_select_mode)
                        {
                            const auto target = cursor_target();
                            if (!target.empty())
                                open_view_selected();
                        }
                        else if (edit_select_mode)
                        {
                            const auto target = cursor_target();
                            if (!target.empty())
                                open_edit_selected();
                        }
                        else if (pending_operation != FileOperation::None && operation_sources.empty())
                        {
                            const auto target = cursor_target();
                            if (!target.empty())
                            {
                                operation_sources.clear();
                                operation_sources.push_back(target);
                                path_edit_text = current_dir.string();
                                SDL_StartTextInput();
                            }
                        }
                        else if (rename_mode && rename_text.empty())
                        {
                            const auto target = cursor_target();
                            if (!target.empty())
                            {
                                rename_text = target.filename().string();
                                rename_scroll_px = 0;
                                SDL_StartTextInput();
                            }
                        }
                        else if (delete_mode && !delete_confirm_mode)
                        {
                            const auto target = cursor_target();
                            if (!target.empty())
                            {
                                delete_confirm_target = target;
                                delete_confirm_choice = 0;
                                delete_confirm_mode = true;
                            }
                        }
                        else if (ev.button.clicks == 2) open_selected(w, h);
                    }
                }
            }
        }

        draw_screen(renderer, w, h);
        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
