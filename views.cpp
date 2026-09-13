/*
 * AI-Attribution License (AIAL) v2 — Draft
 */
#include "simple_commander.h"

void remember_view()
{
    view_history.push_back({special_view, current_dir, tree_selected_path});
}

void enter_special_view(SpecialView view)
{
    remember_view();
    special_view = view;
    if (view == SpecialView::TreeView)
    {
        // Start at the drive root, but expand the path to the current
        // directory and the current directory itself. This gives one level
        // of children immediately without opening the entire drive.
        tree_selected_path = current_dir;
        tree_expanded.clear();
        const auto root = tree_root();
        tree_expanded.insert(root);
        auto p = current_dir;
        while (p != root && p != p.parent_path())
        {
            tree_expanded.insert(p);
            p = p.parent_path();
        }
    }
}

void collect_tree_children(const std::filesystem::path& dir, int depth)
{
    std::error_code ec;
    std::vector<std::filesystem::path> dirs;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec) break;
        std::error_code dec;
        if (e.is_directory(dec) && !dec)
            dirs.push_back(e.path());
    }
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) {
        return lower(a.filename().string()) < lower(b.filename().string());
    });

    for (const auto& child : dirs)
    {
        tree_rows.emplace_back(child, depth);
        if (tree_expanded.count(child))
            collect_tree_children(child, depth + 1);
    }
}

std::filesystem::path tree_root()
{
    return current_dir.root_path();
}

void build_tree()
{
    tree_rows.clear();
    const auto root = tree_root();
    tree_rows.emplace_back(root, 0);
    if (tree_expanded.count(root))
        collect_tree_children(root, 1);

    if (tree_selected_path.empty() ||
        tree_selected_path.root_path() != root.root_path())
        tree_selected_path = current_dir;

    // Rebuild using the current expansion state. The expansion state is
    // deliberately compact so Tree does not explode into the whole drive.
    tree_rows.clear();
    tree_rows.emplace_back(root, 0);
    if (tree_expanded.count(root)) collect_tree_children(root, 1);
}

void collapse_outside_parent(const std::filesystem::path& dir)
{
    const auto parent = dir.parent_path();
    for (auto it = tree_expanded.begin(); it != tree_expanded.end(); )
    {
        const auto& p = *it;
        if (p != dir && p != parent && p.parent_path() != parent && p != tree_root())
            it = tree_expanded.erase(it);
        else
            ++it;
    }
}

void expand_tree_directory(const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) return;
    collapse_outside_parent(dir);
    tree_expanded.insert(dir);
    build_tree();
}

bool is_descendant_path(const std::filesystem::path& child,
                               const std::filesystem::path& parent)
{
    auto p = child;
    while (p != p.parent_path())
    {
        p = p.parent_path();
        if (p == parent) return true;
    }
    return false;
}

void collapse_tree_directory(const std::filesystem::path& dir)
{
    if (dir == tree_root())
    {
        tree_expanded.clear();
    }
    else
    {
        for (auto it = tree_expanded.begin(); it != tree_expanded.end(); )
        {
            if (*it == dir || is_descendant_path(*it, dir))
                it = tree_expanded.erase(it);
            else
                ++it;
        }
        tree_selected_path = dir.parent_path();
    }
    build_tree();
}

int tree_index_of(const std::filesystem::path& p)
{
    for (int i = 0; i < (int)tree_rows.size(); ++i)
        if (tree_rows[i].first == p) return i;
    return -1;
}

void build_drives()
{
    drive_rows.clear();
    DWORD mask = GetLogicalDrives();
    if (mask != 0)
        for (int i = 0; i < 26; ++i)
            if (mask & (1u << i)) drive_rows.emplace_back(std::string(1, char('A' + i)) + ":\\");
}

void previous_view(int w, int h)
{
    if (view_history.empty()) return;

    ViewState state = view_history.back();
    view_history.pop_back();
    special_view = state.view;
    current_dir = state.directory;
    tree_selected_path = state.tree_selected;

    if (special_view == SpecialView::FileView)
    {
        selected = 0;
        page = 0;
        reload_directory();
        normalize_selection(w, h);
    }
    else if (special_view == SpecialView::TreeView)
    {
        build_tree();
        const int main_top = 104;
        const int bottom_h = 32;
        const int main_bottom = h - bottom_h;
        const int main_h = std::max(36, main_bottom - main_top);
        const int max_rows = std::max(1, (main_h - 18) / 18);
        int idx = tree_index_of(tree_selected_path);
        if (idx < 0) idx = 0;
        if (idx < tree_scroll) tree_scroll = idx;
        if (idx >= tree_scroll + max_rows) tree_scroll = idx - max_rows + 1;
        tree_scroll = std::clamp(tree_scroll, 0,
                                 std::max(0, (int)tree_rows.size() - max_rows));
    }
    else
    {
        build_drives();
        selected = 0;
    }
}

void toggle_selected_file()
{
    if (special_view != SpecialView::FileView) return;
    if (has_parent_dir() && selected == 0) return;

    const int index = selected - (has_parent_dir() ? 1 : 0);
    if (index < 0 || index >= (int)entries.size()) return;

    const auto& e = entries[index];
    std::error_code ec;
    if (e.is_directory(ec) || ec) return;

    const auto path = e.path();
    auto it = selected_files.find(path);
    if (it == selected_files.end())
        selected_files.insert(path);
    else
        selected_files.erase(it);
}

void select_all_files()
{
    if (special_view != SpecialView::FileView) return;
    for (const auto& e : entries)
    {
        std::error_code ec;
        if (!e.is_directory(ec) && !ec)
            selected_files.insert(e.path());
    }
}

void deselect_all_files()
{
    if (special_view != SpecialView::FileView) return;
    for (const auto& e : entries)
    {
        std::error_code ec;
        if (!e.is_directory(ec) && !ec)
            selected_files.erase(e.path());
    }
}

void draw_tree(SDL_Renderer* r, int /*w*/, int /*h*/, int file_right,
                      int main_top, int main_bottom)
{
    build_tree();

    const int row_h = 18;
    const int first_y = main_top + 14;
    const int max_rows = std::max(1, (main_bottom - main_top - 18) / row_h);
    int idx = tree_index_of(tree_selected_path);
    if (idx < 0) idx = 0;
    if (idx < tree_scroll) tree_scroll = idx;
    if (idx >= tree_scroll + max_rows) tree_scroll = idx - max_rows + 1;
    tree_scroll = std::clamp(tree_scroll, 0,
                             std::max(0, (int)tree_rows.size() - max_rows));

    for (int i = tree_scroll; i < (int)tree_rows.size() && i < tree_scroll + max_rows; ++i)
    {
        const auto& node = tree_rows[i];
        std::string name = node.second == 0
            ? node.first.string()
            : node.first.filename().string();
        std::string prefix;
        if (node.second > 0)
        {
            for (int d = 1; d < node.second; ++d) prefix += "|   ";
            prefix += (tree_expanded.count(node.first) ? "----" : "+---");
        }

        const bool cur = node.first == tree_selected_path;
        if (cur)
            fill(r, 16, first_y + (i - tree_scroll) * row_h - 1,
                 std::max(100, file_right - 32), 17, ORANGE);
        text(r, prefix + name, 20,
             first_y + (i - tree_scroll) * row_h,
             cur ? BLACK : ORANGE);
    }
}

void draw_drives(SDL_Renderer* r, int file_right, int main_top, int main_bottom)
{
    const int first_y = main_top + 14;
    const int row_h = 18;
    const int max_rows = std::max(1, (main_bottom - main_top - 18) / row_h);

    if (selected >= (int)drive_rows.size()) selected = std::max(0, (int)drive_rows.size() - 1);

    for (int i = 0; i < (int)drive_rows.size() && i < max_rows; ++i)
    {
        const bool cur = i == selected;
        if (cur)
            fill(r, 16, first_y + i * row_h - 1,
                 std::max(100, file_right - 32), 17, ORANGE);
        text(r, drive_rows[i].string(), 20, first_y + i * row_h,
             cur ? BLACK : ORANGE);
    }
}

std::string fit_text_to_width(const std::string& s, int max_width)
{
    if (max_width <= 0) return {};
    if (text_width(s) <= max_width) return s;

    std::string out = s;
    while (!out.empty() && text_width(out) > max_width)
        out.pop_back();
    return out;
}

