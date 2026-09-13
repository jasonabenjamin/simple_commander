/* AI-Attribution License (AIAL) v2 — Draft */
#include "simple_commander.h"
#include <cstring>

std::size_t utf8_prev(const std::string& s, std::size_t pos)
{
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) --pos;
    return pos;
}

std::size_t utf8_next(const std::string& s, std::size_t pos)
{
    if (pos >= s.size()) return s.size();
    ++pos;
    while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) ++pos;
    return pos;
}

bool load_editor_file(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    editor_trailing_newline = !content.empty() && content.back() == '\n';
    editor_lines.clear();
    std::size_t start = 0;
    while (start <= content.size())
    {
        const std::size_t end = content.find('\n', start);
        if (end == std::string::npos)
        {
            std::string line_text = content.substr(start);
            if (!line_text.empty() && line_text.back() == '\r') line_text.pop_back();
            editor_lines.push_back(line_text);
            break;
        }
        std::string line_text = content.substr(start, end - start);
        if (!line_text.empty() && line_text.back() == '\r') line_text.pop_back();
        editor_lines.push_back(line_text);
        start = end + 1;
        if (start == content.size()) break;
    }
    if (editor_lines.empty()) editor_lines.push_back("");
    editor_path = p;
    editor_line = 0;
    editor_column = 0;
    editor_scroll = 0;
    editor_hscroll = 0;
    editor_status = "Read OK";
    editor_dirty = false;
    editor_save_prompt = false;
    editor_save_choice = 0;
    return true;
}

bool save_editor_file()
{
    std::ofstream out(editor_path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (std::size_t i = 0; i < editor_lines.size(); ++i)
    {
        out << editor_lines[i];
        if (i + 1 < editor_lines.size() || editor_trailing_newline) out << '\n';
    }
    if (!out) return false;
    editor_dirty = false;
    editor_status = "Written";
    return true;
}

bool open_edit_selected()
{
    if (special_view != SpecialView::FileView) return false;
    if (has_parent_dir() && selected == 0) return false;
    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index < 0 || index >= (int)entries.size()) return false;
    std::error_code ec;
    const auto& entry = entries[index];
    if (entry.is_directory(ec) || ec) return false;
    if (!load_editor_file(entry.path())) return false;
    editor_mode = true;
    edit_select_mode = false;
    SDL_StartTextInput();
    return true;
}

void begin_edit()
{
    if (special_view != SpecialView::FileView) return;
    edit_select_mode = true;
    view_select_mode = false;
    operation_error.clear();
    path_error = false;
    SDL_StopTextInput();
}

void finish_edit()
{
    editor_mode = false;
    editor_path.clear();
    editor_lines.clear();
    editor_line = 0;
    editor_column = 0;
    editor_scroll = 0;
    editor_hscroll = 0;
    editor_status.clear();
    editor_dirty = false;
    editor_save_prompt = false;
    SDL_StopTextInput();
}

void insert_editor_text(const char* utf8)
{
    if (!utf8 || !*utf8 || editor_line < 0 || editor_line >= (int)editor_lines.size()) return;
    auto& line_text = editor_lines[editor_line];
    line_text.insert(editor_column, utf8);
    editor_column += std::strlen(utf8);
    editor_dirty = true;
    editor_status = "Modified";
}

void editor_backspace()
{
    auto& line_text = editor_lines[editor_line];
    if (editor_column > 0)
    {
        const std::size_t prev = utf8_prev(line_text, editor_column);
        line_text.erase(prev, editor_column - prev);
        editor_column = prev;
        editor_dirty = true;
        editor_status = "Modified";
    }
    else if (editor_line > 0)
    {
        const std::size_t previous_length = editor_lines[editor_line - 1].size();
        editor_lines[editor_line - 1] += line_text;
        editor_lines.erase(editor_lines.begin() + editor_line);
        --editor_line;
        editor_column = previous_length;
        editor_dirty = true;
        editor_status = "Modified";
    }
}

void editor_delete()
{
    auto& line_text = editor_lines[editor_line];
    if (editor_column < line_text.size())
    {
        const std::size_t next = utf8_next(line_text, editor_column);
        line_text.erase(editor_column, next - editor_column);
        editor_dirty = true;
        editor_status = "Modified";
    }
    else if (editor_line + 1 < (int)editor_lines.size())
    {
        line_text += editor_lines[editor_line + 1];
        editor_lines.erase(editor_lines.begin() + editor_line + 1);
        editor_dirty = true;
        editor_status = "Modified";
    }
}

void editor_newline()
{
    auto& line_text = editor_lines[editor_line];
    const bool was_last_line = editor_line + 1 == (int)editor_lines.size();
    const bool was_at_end = editor_column == line_text.size();
    std::string remainder = line_text.substr(editor_column);
    line_text.erase(editor_column);
    editor_lines.insert(editor_lines.begin() + editor_line + 1, remainder);
    ++editor_line;
    editor_column = 0;
    if (was_last_line && was_at_end) editor_trailing_newline = true;
    editor_dirty = true;
    editor_status = "Modified";
}

void editor_delete_line()
{
    if (editor_lines.empty()) return;
    editor_lines.erase(editor_lines.begin() + editor_line);
    if (editor_lines.empty()) editor_lines.push_back("");
    if (editor_line >= (int)editor_lines.size())
        editor_line = (int)editor_lines.size() - 1;
    editor_column = std::min(editor_column, editor_lines[editor_line].size());
    editor_dirty = true;
    editor_status = "Modified";
}

void editor_insert_line()
{
    editor_lines.insert(editor_lines.begin() + editor_line, "");
    editor_column = 0;
    editor_dirty = true;
    editor_status = "Modified";
}

void draw_editor(SDL_Renderer* r, int w, int main_top, int main_bottom)
{
    fill(r, 8, main_top, w - 16, main_bottom - main_top, BLACK);
    const int first_y = main_top + 10;
    const int row_h = 18;
    const int rows = std::max(1, (main_bottom - first_y - 4) / row_h);
    const int max_scroll = std::max(0, (int)editor_lines.size() - rows);
    if (editor_line < editor_scroll) editor_scroll = editor_line;
    if (editor_line >= editor_scroll + rows) editor_scroll = editor_line - rows + 1;
    editor_scroll = std::clamp(editor_scroll, 0, max_scroll);

    const int viewport_width = std::max(1, w - 32);
    const int cursor_content_x = text_width(editor_lines[editor_line].substr(0, editor_column));
    if (cursor_content_x - editor_hscroll > viewport_width - 8)
        editor_hscroll = std::max(0, cursor_content_x - (viewport_width - 8));
    else if (cursor_content_x < editor_hscroll)
        editor_hscroll = cursor_content_x;

    const int full_line_width = text_width(editor_lines[editor_line]);
    if (full_line_width < editor_hscroll)
        editor_hscroll = full_line_width;

    SDL_Rect clip{12, first_y, viewport_width + 4, main_bottom - first_y};
    SDL_RenderSetClipRect(r, &clip);
    for (int i = 0; i < rows && editor_scroll + i < (int)editor_lines.size(); ++i)
    {
        const int line_index = editor_scroll + i;
        const int y = first_y + i * row_h;
        const std::string& line_text = editor_lines[line_index];
        const int x = 16 - editor_hscroll;
        text_utf8(r, line_text, x, y, line_index == editor_line ? CYAN : WHITE);
        text_utf8(r, "▄", x + text_width(line_text), y,
                  line_index == editor_line ? CYAN : WHITE);
        if (line_index == editor_line)
        {
            const int cursor_x = x + cursor_content_x;
            fill(r, cursor_x, y + 1, 2, 14, CYAN);
        }
    }
    SDL_RenderSetClipRect(r, nullptr);
}

void handle_editor_prompt_key(SDL_Keycode k)
{
    if (k == SDLK_UP) editor_save_choice = std::max(0, editor_save_choice - 1);
    else if (k == SDLK_DOWN) editor_save_choice = std::min(2, editor_save_choice + 1);
    else if (k == SDLK_ESCAPE) editor_save_prompt = false;
    else if (k == SDLK_RETURN)
    {
        if (editor_save_choice == 0) finish_edit();
        else if (editor_save_choice == 1)
        {
            if (save_editor_file()) finish_edit();
        }
        else editor_save_prompt = false;
    }
}

