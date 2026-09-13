/*
 * AI-Attribution License (AIAL) v2 — Draft
 */
#include "simple_commander.h"

void begin_view()
{
    if (special_view != SpecialView::FileView) return;
    view_select_mode = true;
    edit_select_mode = false;
    operation_error.clear();
    path_error = false;
    SDL_StopTextInput();
}

void finish_view()
{
    viewer_mode = false;
    viewer_path.clear();
    viewer_lines.clear();
    viewer_scroll = 0;
    SDL_StartTextInput();
}

std::filesystem::path help_file_path()
{
    const char* base = SDL_GetBasePath();
    if (base && *base)
        return std::filesystem::path(base) / "SCOMHELP.TXT";
    return std::filesystem::path("SCOMHELP.TXT");
}

bool begin_help()
{
    const auto path = help_file_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    viewer_lines.clear();
    std::string line_text;
    while (std::getline(in, line_text))
    {
        if (!line_text.empty() && line_text.back() == '\r') line_text.pop_back();
        viewer_lines.push_back(line_text);
    }
    if (viewer_lines.empty()) viewer_lines.push_back("");
    viewer_path = path;
    viewer_scroll = 0;
    viewer_mode = true;
    SDL_StopTextInput();
    return true;
}

static std::vector<std::string> wrap_viewer_line(const std::string& line, int width)
{
    std::vector<std::string> result;
    if (line.empty())
    {
        result.push_back("");
        return result;
    }

    std::size_t start = 0;
    while (start < line.size())
    {
        std::size_t pos = start;
        std::size_t last_space = std::string::npos;

        while (pos < line.size())
        {
            const std::size_t next = utf8_next(line, pos);
            if (text_width(line.substr(start, next - start)) > width)
                break;
            if (line[pos] == ' ' || line[pos] == '\t')
                last_space = pos;
            pos = next;
        }

        if (pos >= line.size())
        {
            result.push_back(line.substr(start));
            break;
        }

        std::size_t cut = (last_space != std::string::npos && last_space > start)
                        ? last_space : pos;

        if (cut == start)
        {
            const std::size_t next = utf8_next(line, start);
            result.push_back(line.substr(start, next - start));
            start = next;
        }
        else
        {
            result.push_back(line.substr(start, cut - start));
            start = cut;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
                ++start;
        }
    }

    return result;
}

static std::vector<std::string> wrapped_viewer_lines(int width)
{
    std::vector<std::string> wrapped;
    for (const auto& line : viewer_lines)
    {
        auto parts = wrap_viewer_line(line, width);
        wrapped.insert(wrapped.end(), parts.begin(), parts.end());
    }
    if (wrapped.empty()) wrapped.push_back("");
    return wrapped;
}

int viewer_visual_row_count(int w)
{
    const int width = std::max(1, w - 32);
    int count = 0;
    for (const auto& line : viewer_lines)
        count += (int)wrap_viewer_line(line, width).size();
    return std::max(1, count);
}

void draw_viewer(SDL_Renderer* r, int w, int file_right, int main_top, int main_bottom)
{
    (void)file_right;
    fill(r, 8, main_top, w - 16, main_bottom - main_top, BLACK);

    const int first_y = main_top + 14;
    const int row_h = 18;
    const int rows = std::max(1, (main_bottom - first_y - 4) / row_h);
    const int total_visual_rows = viewer_visual_row_count(w);
    const int max_scroll = std::max(0, total_visual_rows - rows);
    viewer_scroll = std::clamp(viewer_scroll, 0, max_scroll);

    const auto wrapped = wrapped_viewer_lines(std::max(1, w - 32));

    SDL_Rect clip{12, first_y, std::max(1, w - 24), main_bottom - first_y};
    SDL_RenderSetClipRect(r, &clip);
    for (int i = 0; i < rows && viewer_scroll + i < (int)wrapped.size(); ++i)
        text_utf8(r, wrapped[viewer_scroll + i], 16, first_y + i * row_h, WHITE);
    SDL_RenderSetClipRect(r, nullptr);
}
