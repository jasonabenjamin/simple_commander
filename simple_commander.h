/*
 * Simple Commander - shared declarations
 * AI Freedom License (AIFL) 1.0
 */
#ifndef SIMPLE_COMMANDER_H
#define SIMPLE_COMMANDER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <cstdlib>

constexpr int START_W = 640;
constexpr int START_H = 400;
constexpr int MIN_W = 640;
constexpr int MIN_H = 400;

struct ColorScheme
{
    SDL_Color background;
    SDL_Color panel;
    SDL_Color black;
    SDL_Color accent;
    SDL_Color executable;
    SDL_Color text;
    SDL_Color highlight;
    SDL_Color directory;
};

extern const ColorScheme COLOR_SCHEMES[6];
extern const int COLOR_SCHEME_COUNT;
extern int color_scheme_index;

extern SDL_Color GREEN_BG;
extern SDL_Color PANEL;
extern SDL_Color BLACK;
extern SDL_Color CYAN;
extern SDL_Color GREEN;
extern SDL_Color WHITE;
extern SDL_Color YELLOW;
extern SDL_Color ORANGE;
extern SDL_Color SELECT_DIR;
extern SDL_Color SELECT_EXE;
extern SDL_Color SELECT_FILE;

// Error messages deliberately keep the same high-visibility colors in every scheme.
extern const SDL_Color ERROR_RED;
extern const SDL_Color ERROR_YELLOW;

extern TTF_Font* font;
extern std::filesystem::path current_dir;
extern std::vector<std::filesystem::directory_entry> entries;
extern int selected;
extern int page;
extern bool filter_mode;
extern bool show_hidden;
extern bool filter_ignore_slash;
extern std::string filter_text;
extern bool path_edit_mode;
extern std::string path_edit_text;
extern bool rename_mode;
extern bool view_select_mode;
extern bool edit_select_mode;
extern bool viewer_mode;
extern std::filesystem::path viewer_path;
extern std::vector<std::string> viewer_lines;
extern int viewer_scroll;
extern bool editor_mode;
extern std::filesystem::path editor_path;
extern std::vector<std::string> editor_lines;
extern int editor_line;
extern std::size_t editor_column;
extern int editor_scroll;
extern int editor_hscroll;
extern std::string editor_status;
extern bool editor_dirty;
extern bool editor_save_prompt;
extern int editor_save_choice;
extern bool editor_trailing_newline;
extern bool mkdir_mode;
extern std::string mkdir_text;
extern std::string rename_text;
extern int rename_scroll_px;
extern bool delete_mode;
extern bool delete_confirm_mode;
extern std::filesystem::path delete_confirm_target;
extern int delete_confirm_choice;
extern std::chrono::steady_clock::time_point path_error_until;
extern bool path_error;
extern std::string startup_free_memory;
extern std::string startup_free_drive;
extern std::set<std::filesystem::path> selected_files;

enum class FileOperation { None, Copy, Move };
extern FileOperation pending_operation;
extern std::vector<std::filesystem::path> operation_sources;
extern std::string operation_error;

enum class SpecialView { FileView, TreeView, DrivesView };
extern SpecialView special_view;

struct ViewState
{
    SpecialView view = SpecialView::FileView;
    std::filesystem::path directory;
    std::filesystem::path tree_selected;
};

extern std::vector<ViewState> view_history;
extern std::filesystem::path tree_selected_path;
extern std::vector<std::pair<std::filesystem::path, int>> tree_rows;
extern std::vector<std::filesystem::path> drive_rows;
extern std::map<std::filesystem::path, std::filesystem::path> drive_current_dirs;
extern std::set<std::filesystem::path> tree_expanded;
extern int tree_scroll;

void apply_color_scheme();
void cycle_color_scheme();
bool has_parent_dir();
int logical_count();

void fill(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c);
void line(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c);
void text(SDL_Renderer* r, const std::string& s, int x, int y, SDL_Color c);
void text_utf8(SDL_Renderer* r, const std::string& s, int x, int y, SDL_Color c);
int text_width(const std::string& s);
void centered_text(SDL_Renderer* r, const std::string& s, int left, int right, int y, SDL_Color c);
void right_text(SDL_Renderer* r, const std::string& s, int right, int y, SDL_Color c);
void up_arrow(SDL_Renderer* r, int cx, int cy, SDL_Color c);
void down_arrow(SDL_Renderer* r, int cx, int cy, SDL_Color c);

bool load_font();
std::string lower(std::string s);
bool is_hidden(const std::filesystem::directory_entry& e);
bool matches_filter(const std::filesystem::directory_entry& e);
void reload_directory();
std::string format_size(std::uintmax_t n);
std::string current_time_24h();
std::string free_memory_text();
std::string free_drive_text(const std::filesystem::path& p);
std::string date_time(const std::filesystem::directory_entry& e);
std::string display_stem(const std::filesystem::path& p, int width);
std::string display_ext(const std::filesystem::path& p, int width);
bool is_executable(const std::filesystem::directory_entry& e);
std::string file_attributes(const std::filesystem::directory_entry& e);
int visible_rows(int h);
int visible_columns(int w);
bool has_parent();
int item_count();
int page_capacity(int w, int h);
int page_count(int w, int h);
void normalize_selection(int w, int h);
void draw_file_entry(SDL_Renderer* r, int logical, int x, int y, int name_width, bool cursor_selected);

void remember_view();
std::filesystem::path tree_root();
void enter_special_view(SpecialView view);
void collect_tree_children(const std::filesystem::path& dir, int depth);
void build_tree();
void collapse_outside_parent(const std::filesystem::path& dir);
void expand_tree_directory(const std::filesystem::path& dir);
bool is_descendant_path(const std::filesystem::path& child, const std::filesystem::path& parent);
void collapse_tree_directory(const std::filesystem::path& dir);
int tree_index_of(const std::filesystem::path& p);
void build_drives();
void previous_view(int w, int h);
void toggle_selected_file();
void select_all_files();
void deselect_all_files();
void draw_tree(SDL_Renderer* r, int w, int h, int file_right, int main_top, int main_bottom);
void draw_drives(SDL_Renderer* r, int file_right, int main_top, int main_bottom);
std::string fit_text_to_width(const std::string& s, int max_width);
void draw_screen(SDL_Renderer* r, int w, int h);

std::vector<std::filesystem::path> operation_targets();
std::filesystem::path cursor_target();
std::string operation_label();
void begin_rename();
void finish_rename();
void begin_delete_mode();
void refresh_after_operation();
bool is_console_program(const std::filesystem::path& p);
void execute_path(const std::filesystem::path& p);
void begin_copy_move(FileOperation op);
void finish_copy_move();
void go_parent(int w, int h);
bool open_view_selected();

void begin_view();
void finish_view();
std::filesystem::path help_file_path();
bool begin_help();
void draw_viewer(SDL_Renderer* r, int w, int file_right, int main_top, int main_bottom);
int viewer_visual_row_count(int w);

std::size_t utf8_prev(const std::string& s, std::size_t pos);
std::size_t utf8_next(const std::string& s, std::size_t pos);
bool load_editor_file(const std::filesystem::path& p);
bool save_editor_file();
bool open_edit_selected();
void begin_edit();
void finish_edit();
void insert_editor_text(const char* utf8);
void editor_backspace();
void editor_delete();
void editor_newline();
void editor_delete_line();
void editor_insert_line();
void draw_editor(SDL_Renderer* r, int w, int main_top, int main_bottom);
void handle_editor_prompt_key(SDL_Keycode k);

void begin_mkdir();
void finish_mkdir(int w, int h);
void activate_command(int command_index, int w, int h);
void open_selected(int w, int h);

#endif
