/*
 * AI Freedom License (AIFL) 1.0
 */
#include "simple_commander.h"

// Color Scheme 1: Classic Green
// Color Scheme 2: Dark Blue
// Color Scheme 3: Monokai
// Color Scheme 4: Solarized Dark
// Color Scheme 5: Visual Studio Dark
// Color Scheme 6: Light IDE
const ColorScheme COLOR_SCHEMES[6] = {
    { {0,102,0,255},   {88,88,88,255},  {0,0,0,255},       {0,220,220,255}, {0,255,0,255},   {225,225,225,255}, {255,255,0,255}, {255,150,0,255} },
    { {24,30,44,255},  {52,61,79,255},  {12,16,24,255},    {86,180,233,255}, {78,201,176,255}, {220,226,232,255}, {255,214,92,255},  {255,170,80,255} },
    { {39,40,34,255},  {73,72,62,255},  {25,25,22,255},   {102,217,239,255},{166,226,46,255}, {248,248,242,255}, {253,151,31,255}, {253,151,31,255} },
    { {0,43,54,255},   {42,54,59,255},  {0,30,38,255},    {38,170,190,255}, {133,153,0,255}, {238,232,213,255}, {181,137,0,255}, {203,75,22,255} },
    { {30,30,30,255},  {62,62,66,255},  {20,20,20,255},   {78,201,176,255}, {78,201,176,255}, {240,240,240,255}, {255,204,0,255}, {255,166,77,255} },
    { {235,235,235,255},{205,205,205,255},{255,255,255,255},{0,102,153,255}, {0,128,0,255},   {25,25,25,255},    {180,100,0,255}, {170,80,0,255} }
};

const int COLOR_SCHEME_COUNT = 6;
int color_scheme_index = 0;

SDL_Color GREEN_BG;
SDL_Color PANEL;
SDL_Color BLACK;
SDL_Color CYAN;
SDL_Color GREEN;
SDL_Color WHITE;
SDL_Color YELLOW;
SDL_Color ORANGE;
SDL_Color SELECT_DIR;
SDL_Color SELECT_EXE;
SDL_Color SELECT_FILE;

// Error messages deliberately keep the same high-visibility colors in every scheme.
const SDL_Color ERROR_RED = {255, 0, 0, 255};
const SDL_Color ERROR_YELLOW = {255, 255, 0, 255};

TTF_Font* font = nullptr;
std::filesystem::path current_dir = std::filesystem::current_path();
std::vector<std::filesystem::directory_entry> entries;
int selected = 0;
int page = 0;
bool filter_mode = false;
bool show_hidden = false;
bool filter_ignore_slash = false;
std::string filter_text;
bool path_edit_mode = false;
std::string path_edit_text;
bool rename_mode = false;
bool view_select_mode = false;
bool edit_select_mode = false;
bool viewer_mode = false;
std::filesystem::path viewer_path;
std::vector<std::string> viewer_lines;
int viewer_scroll = 0;
bool editor_mode = false;
std::filesystem::path editor_path;
std::vector<std::string> editor_lines;
int editor_line = 0;
std::size_t editor_column = 0;
int editor_scroll = 0;
int editor_hscroll = 0;
std::string editor_status;
bool editor_dirty = false;
bool editor_save_prompt = false;
int editor_save_choice = 0;
bool editor_trailing_newline = false;
bool mkdir_mode = false;
std::string mkdir_text;
std::string rename_text;
int rename_scroll_px = 0;
bool delete_mode = false;
bool delete_confirm_mode = false;
std::filesystem::path delete_confirm_target;
int delete_confirm_choice = 0;
std::chrono::steady_clock::time_point path_error_until{};
bool path_error = false;
std::string startup_free_memory;
std::string startup_free_drive;
std::set<std::filesystem::path> selected_files;

FileOperation pending_operation = FileOperation::None;
std::vector<std::filesystem::path> operation_sources;
std::string operation_error;

SpecialView special_view = SpecialView::FileView;
std::vector<ViewState> view_history;
std::filesystem::path tree_selected_path;
std::vector<std::pair<std::filesystem::path, int>> tree_rows;
std::vector<std::filesystem::path> drive_rows;
std::map<std::filesystem::path, std::filesystem::path> drive_current_dirs;
std::set<std::filesystem::path> tree_expanded;
int tree_scroll = 0;
