/*
 * AI-Attribution License (AIAL) v2 — Draft
 */
#include "simple_commander.h"

void apply_color_scheme()
{
    const auto& c = COLOR_SCHEMES[color_scheme_index];
    GREEN_BG = c.background;
    PANEL = c.panel;
    BLACK = c.black;
    CYAN = c.accent;
    GREEN = c.executable;
    WHITE = c.text;
    YELLOW = c.highlight;
    ORANGE = c.directory;
    SELECT_DIR = c.executable;
    SELECT_EXE = c.executable;
    SELECT_FILE = c.text;
}

void cycle_color_scheme()
{
    color_scheme_index = (color_scheme_index + 1) % COLOR_SCHEME_COUNT;
    apply_color_scheme();
}

bool has_parent_dir()
{
    return current_dir.parent_path() != current_dir;
}

int logical_count()
{
    return static_cast<int>(entries.size()) + (has_parent_dir() ? 1 : 0);
}

void fill(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void line(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

void text(SDL_Renderer* r, const std::string& s, int x, int y, SDL_Color c)
{
    SDL_Surface* surface = TTF_RenderText_Solid(font, s.c_str(), c);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(r, surface);
    if (texture)
    {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(r, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void text_utf8(SDL_Renderer* r, const std::string& s, int x, int y, SDL_Color c)
{
    SDL_Surface* surface = TTF_RenderUTF8_Solid(font, s.c_str(), c);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(r, surface);
    if (texture)
    {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(r, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

int text_width(const std::string& s)
{
    int w = 0, h = 0;
    return TTF_SizeText(font, s.c_str(), &w, &h) == 0 ? w : 0;
}

void centered_text(SDL_Renderer* r, const std::string& s,
                   int left, int right, int y, SDL_Color c)
{
    text(r, s, left + ((right - left) - text_width(s)) / 2, y, c);
}

void right_text(SDL_Renderer* r, const std::string& s, int right, int y, SDL_Color c)
{
    text(r, s, right - text_width(s), y, c);
}

void up_arrow(SDL_Renderer* r, int cx, int cy, SDL_Color c)
{
    line(r, cx, cy + 4, cx, cy - 4, c);
    line(r, cx, cy - 4, cx - 3, cy - 1, c);
    line(r, cx, cy - 4, cx + 3, cy - 1, c);
}

void down_arrow(SDL_Renderer* r, int cx, int cy, SDL_Color c)
{
    line(r, cx, cy - 4, cx, cy + 4, c);
    line(r, cx, cy + 4, cx - 3, cy + 1, c);
    line(r, cx, cy + 4, cx + 3, cy + 1, c);
}

bool load_font()
{
    const char* candidates[] = {
        "C:/Windows/Fonts/cour.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/lucon.ttf"
    };

    for (const char* path : candidates)
    {
        font = TTF_OpenFont(path, 11);
        if (font) return true;
    }
    return false;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool is_hidden(const std::filesystem::directory_entry& e)
{
    const DWORD a = GetFileAttributesW(e.path().wstring().c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool matches_filter(const std::filesystem::directory_entry& e)
{
    if (!show_hidden && is_hidden(e)) return false;
    if (filter_text.empty()) return true;
    return lower(e.path().filename().string()).find(lower(filter_text)) != std::string::npos;
}

void reload_directory()
{
    entries.clear();
    std::error_code ec;

    if (!std::filesystem::is_directory(current_dir, ec)) return;

    for (const auto& e : std::filesystem::directory_iterator(current_dir, ec))
        if (!ec && matches_filter(e)) entries.push_back(e);

    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b)
        {
            std::error_code ea, eb;
            bool ad = a.is_directory(ea);
            bool bd = b.is_directory(eb);
            if (ad != bd) return ad > bd;
            return lower(a.path().filename().string()) <
                   lower(b.path().filename().string());
        });

    selected = std::clamp(selected, 0, std::max(0, logical_count() - 1));
    page = 0;
}

std::string format_size(std::uintmax_t n)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (n < 1024ULL) out << static_cast<double>(n) << " B";
    else if (n < 1024ULL * 1024ULL) out << static_cast<double>(n) / 1024.0 << " KB";
    else if (n < 1024ULL * 1024ULL * 1024ULL) out << static_cast<double>(n) / (1024.0 * 1024.0) << " MB";
    else if (n < 1024ULL * 1024ULL * 1024ULL * 1024ULL) out << static_cast<double>(n) / (1024.0 * 1024.0 * 1024.0) << " GB";
    else out << static_cast<double>(n) / (1024.0 * 1024.0 * 1024.0 * 1024.0) << " TB";
    return out.str();
}

std::string current_time_24h()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
        << std::setw(2) << tm.tm_min << ":" << std::setw(2) << tm.tm_sec;
    return out.str();
}

std::string free_memory_text()
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) return format_size(ms.ullAvailPhys);
    return "--";
}

std::string free_drive_text(const std::filesystem::path& p)
{
    ULARGE_INTEGER free_bytes{};
    if (GetDiskFreeSpaceExW(p.root_path().wstring().c_str(), &free_bytes, nullptr, nullptr))
        return format_size(free_bytes.QuadPart);
    return "--";
}

std::string date_time(const std::filesystem::directory_entry& e)
{
    std::error_code ec;
    auto ft = e.last_write_time(ec);
    if (ec) return "--/--/-- --:--";

    auto st = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - decltype(ft)::clock::now() + std::chrono::system_clock::now());
    std::time_t t = std::chrono::system_clock::to_time_t(st);

    std::tm tm{};
    localtime_s(&tm, &t);

    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(2) << tm.tm_mon + 1 << "-"
        << std::setw(2) << tm.tm_mday << "-"
        << std::setw(2) << tm.tm_year % 100 << " "
        << std::setw(2) << tm.tm_hour << ":"
        << std::setw(2) << tm.tm_min;
    return out.str();
}

std::string display_stem(const std::filesystem::path& p, int width)
{
    std::string s = p.stem().string();
    if (width <= 0) return {};
    if ((int)s.size() > width) s.resize(width);
    return s;
}

std::string display_ext(const std::filesystem::path& p, int width)
{
    std::string s = p.has_extension() ? p.extension().string().substr(1) : "";
    if (width <= 0) return {};
    if ((int)s.size() > width) s.resize(width);
    return s;
}

bool is_executable(const std::filesystem::directory_entry& e)
{
    const std::string ext = lower(e.path().extension().string());
    return ext == ".exe" || ext == ".com" || ext == ".bat" || ext == ".cmd";
}

std::string file_attributes(const std::filesystem::directory_entry& e)
{
    DWORD a = GetFileAttributesW(e.path().wstring().c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return "------";

    std::string result = "------";
    if (a & FILE_ATTRIBUTE_READONLY)  result[0] = 'R';
    if (a & FILE_ATTRIBUTE_DIRECTORY) result[1] = 'D';
    if (a & FILE_ATTRIBUTE_HIDDEN)    result[2] = 'H';
    if (a & FILE_ATTRIBUTE_SYSTEM)    result[3] = 'S';
    if (a & FILE_ATTRIBUTE_ARCHIVE)   result[4] = 'A';
    if (a & FILE_ATTRIBUTE_TEMPORARY) result[5] = 'T';
    return result;
}

int visible_rows(int h)
{
    // Use all available vertical space for file rows. The minimum window
    // is still limited to two columns, but it should not waste vertical space.
    return std::max(2, (h - 118 - 24) / 18);
}

int visible_columns(int w)
{
    const int file_right = w - 79;
    const int usable = file_right - 20;
    // At the minimum 640px window, deliberately show exactly two columns.
    // Additional columns appear only as the window gets wider.
    return std::max(2, usable / 245);
}

bool has_parent()
{
    return current_dir.parent_path() != current_dir;
}

int item_count()
{
    return static_cast<int>(entries.size()) + (has_parent() ? 1 : 0);
}

int page_capacity(int w, int h)
{
    return visible_rows(h) * visible_columns(w);
}

int page_count(int w, int h)
{
    const int cap = page_capacity(w, h);
    return std::max(1, (logical_count() + cap - 1) / cap);
}

void normalize_selection(int w, int h)
{
    const int cap = page_capacity(w, h);
    const int pages = page_count(w, h);
    const int max_logical = std::max(0, logical_count() - 1);

    selected = std::clamp(selected, 0, max_logical);
    page = std::clamp(page, 0, pages - 1);

    // Keep the selected item on the page being displayed.
    page = std::clamp(selected / cap, 0, pages - 1);
}

void draw_file_entry(SDL_Renderer* r, int logical, int x, int y,
                             int name_width, bool cursor_selected)
{
    if (has_parent() && logical == 0)
    {
        SDL_Color c = cursor_selected ? BLACK : ORANGE;
        if (cursor_selected) fill(r, x - 4, y - 1, name_width + 38, 17, ORANGE);
        text(r, "..", x, y, c);
        text(r, "<D>", x + name_width + 8, y, c);
        return;
    }

    const int index = logical - (has_parent() ? 1 : 0);
    if (index < 0 || index >= (int)entries.size()) return;

    const auto& e = entries[index];
    const bool marked = !e.is_directory() && selected_files.count(e.path()) != 0;
    SDL_Color c;

    if (marked)
    {
        // A tagged file is black text on yellow. When the cursor is over it,
        // invert it to yellow text on black so the cursor remains distinct.
        if (cursor_selected)
        {
            fill(r, x - 4, y - 1, name_width + 38, 17, BLACK);
            c = YELLOW;
        }
        else
        {
            fill(r, x - 4, y - 1, name_width + 38, 17, YELLOW);
            c = BLACK;
        }
    }
    else if (e.is_directory())
    {
        c = cursor_selected ? BLACK : ORANGE;
        if (cursor_selected) fill(r, x - 4, y - 1, name_width + 38, 17, ORANGE);
    }
    else if (is_executable(e))
    {
        // Executables are bright green.
        c = cursor_selected ? BLACK : GREEN;
        if (cursor_selected) fill(r, x - 4, y - 1, name_width + 38, 17, GREEN);
    }
    else
    {
        c = cursor_selected ? BLACK : WHITE;
        if (cursor_selected) fill(r, x - 4, y - 1, name_width + 38, 17, WHITE);
    }

    text(r, display_stem(e.path(), name_width / 10), x, y, c);
    text(r, e.is_directory() ? "<D>" : display_ext(e.path(), 3),
         x + name_width + 8, y, c);
}

