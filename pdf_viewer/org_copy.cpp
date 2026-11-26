#include "org_copy.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace {

std::wstring trim_ws(const std::wstring& s) {
    size_t start = 0;
    while (start < s.size() && std::iswspace(s[start])) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::iswspace(s[end - 1])) {
        end--;
    }
    return s.substr(start, end - start);
}

std::wstring strip_trailing_slashes(std::wstring s) {
    while (s.size() > 1 && (s.back() == L'/' || s.back() == L'\\')) {
        s.pop_back();
    }
    return s;
}

bool starts_with_path_prefix(const std::wstring& path, const std::wstring& prefix) {
    if (prefix.empty() || path.size() < prefix.size()) {
        return false;
    }
    if (path.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    if (path.size() == prefix.size()) {
        return true;
    }
    if (prefix == L"/" || prefix == L"\\") {
        return true;
    }
    wchar_t next = path[prefix.size()];
    return next == L'/' || next == L'\\';
}

std::wstring expand_leading_tilde(const std::wstring& p, const std::wstring& home_dir) {
    if (home_dir.empty() || p.empty() || p[0] != L'~') {
        return p;
    }
    std::wstring clean_home = strip_trailing_slashes(home_dir);
    if (p.size() == 1) {
        return clean_home;
    }
    if (p[1] == L'/' || p[1] == L'\\') {
        return clean_home + p.substr(1);
    }
    return p;
}

std::wstring format_float_compact(float v) {
    if (!std::isfinite(v)) {
        return L"0";
    }
    if (std::abs(v) < 1e-6f) {
        return L"0";
    }
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(6) << v;
    std::string s = oss.str();
    return std::wstring(s.begin(), s.end());
}

bool is_bullet_char(wchar_t c) {
    switch (c) {
        case L'\u2022': // •
        case L'\u25E6': // ◦
        case L'\u25AA': // ▪
        case L'\u25AB': // ▫
        case L'\u2023': // ‣
        case L'\u2043': // ⁃
        case L'\u25B8': // ▸
        case L'\u25BA': // ►
        case L'\u25CF': // ●
        case L'\u25CB': // ○
        case L'\u25A0': // ■
        case L'\u25A1': // □
        case L'\u2219': // ∙
        case L'\u00B7': // ·
            return true;
        default:
            return false;
    }
}

bool is_dash_bullet_char(wchar_t c) {
    return c == L'-' || c == L'\u2013' || c == L'\u2014';
}

bool is_org_pre_border(wchar_t c) {
    return c == L'\0' || std::iswspace(c) ||
           c == L'-' || c == L'(' || c == L'{' || c == L'\'' || c == L'"';
}

bool is_org_post_border(wchar_t c) {
    return c == L'\0' || std::iswspace(c) ||
           c == L'-' || c == L'.' || c == L',' || c == L':' || c == L';' ||
           c == L'!' || c == L'?' || c == L'\'' || c == L'"' ||
           c == L')' || c == L'}' || c == L'[';
}

struct StyleKey {
    bool bold = false;
    bool italic = false;
    bool monospace = false;
    bool strikeout = false;

    bool any() const {
        return bold || italic || monospace || strikeout;
    }

    bool operator==(const StyleKey& other) const {
        return bold == other.bold &&
               italic == other.italic &&
               monospace == other.monospace &&
               strikeout == other.strikeout;
    }
};

StyleKey style_of(const OrgStyledChar& ch, bool preserve_styles) {
    if (!preserve_styles) {
        return {};
    }
    return {
        ch.bold,
        ch.italic && !ch.is_math_font,
        ch.monospace,
        ch.strikeout
    };
}

struct RawLine {
    int block_id = 0;
    std::vector<OrgStyledChar> chars;
    bool is_bullet = false;
    bool is_list_item = false;
};

struct LogicalLine {
    std::vector<OrgStyledChar> chars;
    bool starts_with_bullet = false;
    bool is_list_item = false;
    bool blank_line_before = false;
};

bool detect_numbered_list_prefix(const std::vector<OrgStyledChar>& chars, size_t start) {
    size_t i = start;
    size_t digits = 0;
    while (i < chars.size() && std::iswdigit(chars[i].c) && digits < 4) {
        i++;
        digits++;
    }
    if (digits == 0 || i >= chars.size()) {
        return false;
    }
    if (chars[i].c != L'.' && chars[i].c != L')') {
        return false;
    }
    i++;
    return i < chars.size() && std::iswspace(chars[i].c);
}

void normalize_raw_line_bullet(RawLine& line) {
    size_t i = 0;
    while (i < line.chars.size() && std::iswspace(line.chars[i].c)) {
        i++;
    }
    if (i >= line.chars.size()) {
        return;
    }

    bool matched_bullet = false;
    size_t after_bullet = i;
    if (is_bullet_char(line.chars[i].c) && line.chars[i].link_id < 0) {
        matched_bullet = true;
        after_bullet = i + 1;
    } else if (is_dash_bullet_char(line.chars[i].c) && line.chars[i].link_id < 0 &&
               (i + 1 < line.chars.size()) && std::iswspace(line.chars[i + 1].c)) {
        matched_bullet = true;
        after_bullet = i + 1;
    }

    if (matched_bullet) {
        while (after_bullet < line.chars.size() && std::iswspace(line.chars[after_bullet].c)) {
            after_bullet++;
        }
        line.chars.erase(line.chars.begin(), line.chars.begin() + static_cast<std::ptrdiff_t>(after_bullet));
        line.is_bullet = true;
        line.is_list_item = true;
        return;
    }

    if (detect_numbered_list_prefix(line.chars, i)) {
        if (i > 0) {
            line.chars.erase(line.chars.begin(), line.chars.begin() + static_cast<std::ptrdiff_t>(i));
        }
        line.is_list_item = true;
    }
}

bool has_alnum(const std::wstring& s) {
    for (wchar_t c : s) {
        if (std::iswalnum(c)) {
            return true;
        }
    }
    return false;
}

std::wstring wrap_styled_text(
    const std::wstring& text,
    const StyleKey& style,
    wchar_t prev_char,
    wchar_t next_char) {

    if (!style.any() || text.empty()) {
        return text;
    }

    size_t lead = 0;
    while (lead < text.size() && std::iswspace(text[lead])) {
        lead++;
    }
    if (lead == text.size()) {
        return text;
    }
    size_t trail = text.size();
    while (trail > lead && std::iswspace(text[trail - 1])) {
        trail--;
    }

    std::wstring prefix = text.substr(0, lead);
    std::wstring core = text.substr(lead, trail - lead);
    std::wstring suffix = text.substr(trail);

    wchar_t effective_prev = prefix.empty() ? prev_char : prefix.back();
    wchar_t effective_next = suffix.empty() ? next_char : suffix.front();

    if (!is_org_pre_border(effective_prev) || !is_org_post_border(effective_next)) {
        return text;
    }

    if (style.monospace) {
        if (core.find(L'~') == std::wstring::npos) {
            return prefix + L"~" + core + L"~" + suffix;
        }
        if (core.find(L'=') == std::wstring::npos) {
            return prefix + L"=" + core + L"=" + suffix;
        }
        return text;
    }

    if (!has_alnum(core)) {
        return text;
    }

    std::wstring open_markers;
    std::wstring close_markers;

    if (style.bold && core.find(L'*') == std::wstring::npos) {
        open_markers.push_back(L'*');
        close_markers.insert(close_markers.begin(), L'*');
    }
    if (style.italic && core.find(L'/') == std::wstring::npos) {
        open_markers.push_back(L'/');
        close_markers.insert(close_markers.begin(), L'/');
    }
    if (style.strikeout && core.find(L'+') == std::wstring::npos) {
        open_markers.push_back(L'+');
        close_markers.insert(close_markers.begin(), L'+');
    }

    if (open_markers.empty()) {
        return text;
    }

    return prefix + open_markers + core + close_markers + suffix;
}

} // namespace

std::wstring abbreviate_org_path(
    const std::wstring& path,
    const std::wstring& home_dir,
    const std::wstring& abbrevs_spec) {

    if (path.empty()) {
        return path;
    }

    std::vector<std::pair<std::wstring, std::wstring>> rules;
    size_t pos = 0;
    while (pos < abbrevs_spec.size()) {
        size_t sep = abbrevs_spec.find_first_of(L";|\n", pos);
        std::wstring entry = (sep == std::wstring::npos)
            ? abbrevs_spec.substr(pos)
            : abbrevs_spec.substr(pos, sep - pos);
        pos = (sep == std::wstring::npos) ? abbrevs_spec.size() : sep + 1;

        entry = trim_ws(entry);
        if (entry.empty()) {
            continue;
        }
        size_t eq = entry.find(L'=');
        if (eq == std::wstring::npos || eq == 0 || eq + 1 >= entry.size()) {
            continue;
        }
        std::wstring alias = strip_trailing_slashes(trim_ws(entry.substr(0, eq)));
        std::wstring prefix = strip_trailing_slashes(
            expand_leading_tilde(trim_ws(entry.substr(eq + 1)), home_dir));
        if (!alias.empty() && !prefix.empty()) {
            rules.emplace_back(prefix, alias);
        }
    }

    std::sort(rules.begin(), rules.end(), [](const auto& a, const auto& b) {
        return a.first.size() > b.first.size();
    });

    for (const auto& [prefix, alias] : rules) {
        if (starts_with_path_prefix(path, prefix)) {
            if (path.size() == prefix.size()) {
                return alias;
            }
            return alias + path.substr(prefix.size());
        }
    }

    std::wstring clean_home = strip_trailing_slashes(home_dir);
    if (!clean_home.empty() && starts_with_path_prefix(path, clean_home)) {
        if (path.size() == clean_home.size()) {
            return L"~";
        }
        return L"~" + path.substr(clean_home.size());
    }

    return path;
}

std::wstring escape_org_link_target(const std::wstring& target) {
    std::wstring res;
    res.reserve(target.size());
    for (wchar_t c : target) {
        if (c == L'[' || c == L']') {
            res.push_back(L'\\');
        }
        res.push_back(c);
    }
    return res;
}

std::wstring escape_org_link_description(const std::wstring& desc) {
    std::wstring res;
    res.reserve(desc.size());
    bool prev_space = false;
    for (wchar_t c : desc) {
        if (c == L'\r' || c == L'\n' || c == L'\t') {
            if (!prev_space && !res.empty()) {
                res.push_back(L' ');
                prev_space = true;
            }
            continue;
        }
        if (c == L'[') {
            res.push_back(L'{');
            prev_space = false;
        } else if (c == L']') {
            res.push_back(L'}');
            prev_space = false;
        } else {
            res.push_back(c);
            prev_space = (c == L' ');
        }
    }
    return trim_ws(res);
}

std::wstring format_sioyek_v1_link(
    const std::wstring& file_path,
    int page_1based,
    std::optional<float> x,
    std::optional<float> y,
    std::optional<float> zoom,
    const std::wstring& home_dir,
    const std::wstring& abbrevs_spec,
    const std::wstring& description) {

    std::wstring abbrev = abbreviate_org_path(file_path, home_dir, abbrevs_spec);
    std::wstring target = L"sioyek-v1:" + abbrev;

    if (page_1based > 0) {
        target += L"::" + std::to_wstring(page_1based);
        if (x.has_value() && y.has_value()) {
            target += L":" + format_float_compact(*x) + L":" + format_float_compact(*y);
            if (zoom.has_value()) {
                target += L":" + format_float_compact(*zoom);
            }
        }
    }

    std::wstring escaped_target = escape_org_link_target(target);
    std::wstring escaped_desc = escape_org_link_description(description);
    if (escaped_desc.empty()) {
        return L"[[" + escaped_target + L"]]";
    }
    return L"[[" + escaped_target + L"][" + escaped_desc + L"]]";
}

std::wstring format_org_link(
    const OrgLinkTarget& target,
    const std::wstring& description,
    const OrgCopyConfig& config) {

    std::wstring trimmed_desc = trim_ws(description);
    switch (target.kind) {
        case OrgLinkTarget::Kind::External: {
            std::wstring escaped_uri = escape_org_link_target(target.raw_uri);
            std::wstring escaped_desc = escape_org_link_description(trimmed_desc);
            if (escaped_desc.empty() || trimmed_desc == target.raw_uri) {
                return L"[[" + escaped_uri + L"]]";
            }
            return L"[[" + escaped_uri + L"][" + escaped_desc + L"]]";
        }
        case OrgLinkTarget::Kind::Internal: {
            std::optional<float> x_opt;
            std::optional<float> y_opt;
            std::optional<float> zoom_opt;
            if (!target.whole_page) {
                x_opt = target.target_x;
                y_opt = target.target_y;
                if (config.include_zoom_in_internal_links && target.current_zoom > 0.0f) {
                    zoom_opt = target.current_zoom;
                }
            }
            return format_sioyek_v1_link(
                target.file_path,
                target.target_page_1based,
                x_opt,
                y_opt,
                zoom_opt,
                config.home_dir,
                config.path_abbrevs,
                trimmed_desc);
        }
        case OrgLinkTarget::Kind::FilePdf: {
            std::optional<float> x_opt;
            std::optional<float> y_opt;
            std::optional<float> zoom_opt;
            if (!target.whole_page) {
                x_opt = target.target_x;
                y_opt = target.target_y;
            }
            return format_sioyek_v1_link(
                target.file_path,
                target.target_page_1based,
                x_opt,
                y_opt,
                zoom_opt,
                config.home_dir,
                config.path_abbrevs,
                trimmed_desc);
        }
        case OrgLinkTarget::Kind::FileOther: {
            std::wstring abbrev = abbreviate_org_path(target.file_path, config.home_dir, config.path_abbrevs);
            std::wstring escaped_target = escape_org_link_target(L"file:" + abbrev);
            std::wstring escaped_desc = escape_org_link_description(trimmed_desc);
            if (escaped_desc.empty()) {
                return L"[[" + escaped_target + L"]]";
            }
            return L"[[" + escaped_target + L"][" + escaped_desc + L"]]";
        }
        case OrgLinkTarget::Kind::None:
        default:
            return description;
    }
}

bool is_likely_math_font_name(const std::string& font_name) {
    if (font_name.empty()) {
        return false;
    }
    std::string lower;
    lower.reserve(font_name.size());
    for (unsigned char c : font_name) {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }
    // Strip PDF subset prefix like "ABCDEF+"
    size_t plus_pos = lower.find('+');
    if (plus_pos != std::string::npos && plus_pos + 1 < lower.size()) {
        lower = lower.substr(plus_pos + 1);
    }

    static const char* kMathPatterns[] = {
        "math", "cmmi", "cmsy", "cmex", "msam", "msbm",
        "mtmi", "mtsy", "mtex", "symbol", "eurm", "eufm",
        "eusm", "euex", "txmi", "pxmi", "zplm", "ztmmi",
        "rtxmi", "newtxmi", "lmsy", "lmmi", "lmex"
    };
    for (const char* pat : kMathPatterns) {
        if (lower.find(pat) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::wstring serialize_selection_to_org(
    const std::vector<OrgStyledChar>& chars,
    const std::vector<OrgLinkTarget>& links,
    const OrgCopyConfig& config) {

    if (chars.empty()) {
        return L"";
    }

    // 1. Group characters into raw lines preserving block_id and line_id.
    std::vector<RawLine> raw_lines;
    for (size_t i = 0; i < chars.size(); i++) {
        const OrgStyledChar& ch = chars[i];
        bool start_new = raw_lines.empty() ||
                         ch.block_id != raw_lines.back().block_id ||
                         (i > 0 && (chars[i - 1].line_end || ch.line_id != chars[i - 1].line_id));
        if (start_new) {
            raw_lines.push_back(RawLine{ch.block_id, {}, false, false});
        }
        raw_lines.back().chars.push_back(ch);
    }

    // Trim trailing spaces on each raw line and normalize leading bullets.
    std::vector<RawLine> cleaned_lines;
    cleaned_lines.reserve(raw_lines.size());
    for (auto& rl : raw_lines) {
        while (!rl.chars.empty() && rl.chars.back().c == L' ') {
            rl.chars.pop_back();
        }
        if (rl.chars.empty()) {
            continue;
        }
        normalize_raw_line_bullet(rl);
        if (!rl.chars.empty() || rl.is_bullet) {
            cleaned_lines.push_back(std::move(rl));
        }
    }

    if (cleaned_lines.empty()) {
        return L"";
    }

    // 2. Merge raw lines into logical lines (reflowing wrapped lines within a paragraph).
    std::vector<LogicalLine> logical_lines;
    for (size_t i = 0; i < cleaned_lines.size(); i++) {
        const RawLine& rl = cleaned_lines[i];
        if (i == 0) {
            logical_lines.push_back(LogicalLine{rl.chars, rl.is_bullet, rl.is_list_item, false});
            continue;
        }

        const RawLine& prev_rl = cleaned_lines[i - 1];
        bool block_changed = (rl.block_id != prev_rl.block_id);
        bool force_break = block_changed || rl.is_list_item || config.add_newlines;

        if (force_break) {
            bool blank_before = block_changed && !(logical_lines.back().is_list_item && rl.is_list_item);
            logical_lines.push_back(LogicalLine{rl.chars, rl.is_bullet, rl.is_list_item, blank_before});
            continue;
        }

        // Reflow onto the previous logical line.
        auto& dst = logical_lines.back().chars;
        if (!dst.empty() && !rl.chars.empty()) {
            wchar_t last_c = dst.back().c;
            bool soft_hyphen = (last_c == L'\u00AD');
            bool ascii_hyphen = (last_c == L'-') &&
                                dst.size() >= 2 &&
                                std::iswalnum(dst[dst.size() - 2].c) &&
                                std::iswalnum(rl.chars.front().c);
            if (soft_hyphen || ascii_hyphen) {
                dst.pop_back();
            } else if (!std::iswspace(last_c) && !std::iswspace(rl.chars.front().c)) {
                OrgStyledChar space_ch;
                space_ch.c = L' ';
                const auto& a = dst.back();
                const auto& b = rl.chars.front();
                if (a.link_id >= 0 && a.link_id == b.link_id) {
                    space_ch.link_id = a.link_id;
                }
                space_ch.bold = a.bold && b.bold;
                space_ch.italic = a.italic && b.italic;
                space_ch.monospace = a.monospace && b.monospace;
                space_ch.strikeout = a.strikeout && b.strikeout;
                space_ch.is_math_font = a.is_math_font && b.is_math_font;
                dst.push_back(space_ch);
            }
        }
        size_t start_idx = 0;
        while (start_idx < rl.chars.size() && std::iswspace(rl.chars[start_idx].c)) {
            start_idx++;
        }
        dst.insert(dst.end(), rl.chars.begin() + static_cast<std::ptrdiff_t>(start_idx), rl.chars.end());
    }

    // 3. Emit each logical line.
    std::wstring output;
    for (size_t li = 0; li < logical_lines.size(); li++) {
        if (li > 0) {
            output.push_back(L'\n');
            if (logical_lines[li].blank_line_before) {
                output.push_back(L'\n');
            }
        }

        auto& line_chars = logical_lines[li].chars;

        // Trim leading and trailing whitespace on the logical line.
        size_t trim_begin = 0;
        while (trim_begin < line_chars.size() && std::iswspace(line_chars[trim_begin].c)) {
            trim_begin++;
        }
        size_t trim_end = line_chars.size();
        while (trim_end > trim_begin && std::iswspace(line_chars[trim_end - 1].c)) {
            trim_end--;
        }
        if (trim_begin > 0 || trim_end < line_chars.size()) {
            line_chars = std::vector<OrgStyledChar>(
                line_chars.begin() + static_cast<std::ptrdiff_t>(trim_begin),
                line_chars.begin() + static_cast<std::ptrdiff_t>(trim_end));
        }

        // Clear invalid link_ids (out of bounds or Kind::None).
        for (auto& ch : line_chars) {
            if (ch.link_id < 0 || static_cast<size_t>(ch.link_id) >= links.size() ||
                links[static_cast<size_t>(ch.link_id)].kind == OrgLinkTarget::Kind::None) {
                ch.link_id = -1;
            }
        }

        // Bridge whitespace between two chars that share the same link_id.
        for (size_t i = 0; i < line_chars.size(); i++) {
            if (!std::iswspace(line_chars[i].c) || line_chars[i].link_id >= 0) {
                continue;
            }
            size_t j = i;
            while (j < line_chars.size() && std::iswspace(line_chars[j].c) && line_chars[j].link_id < 0) {
                j++;
            }
            if (i > 0 && j < line_chars.size() &&
                line_chars[i - 1].link_id >= 0 &&
                line_chars[i - 1].link_id == line_chars[j].link_id) {
                int shared = line_chars[i - 1].link_id;
                for (size_t k = i; k < j; k++) {
                    line_chars[k].link_id = shared;
                }
            }
            i = j;
        }

        // Push whitespace at the edges of a link run outside the link.
        for (size_t i = 0; i < line_chars.size(); ) {
            if (line_chars[i].link_id < 0) {
                i++;
                continue;
            }
            int lid = line_chars[i].link_id;
            size_t j = i;
            while (j < line_chars.size() && line_chars[j].link_id == lid) {
                j++;
            }
            size_t s = i;
            while (s < j && std::iswspace(line_chars[s].c)) {
                line_chars[s].link_id = -1;
                s++;
            }
            size_t e = j;
            while (e > s && std::iswspace(line_chars[e - 1].c)) {
                line_chars[e - 1].link_id = -1;
                e--;
            }
            i = j;
        }

        // Single-character italic guard for isolated math variables (e.g. " x ")
        // and math-font italic suppression.
        if (config.preserve_styles) {
            for (auto& ch : line_chars) {
                if (ch.is_math_font) {
                    ch.italic = false;
                }
            }

            // Bridge shared styles across spaces between identically styled chars.
            for (size_t i = 0; i < line_chars.size(); i++) {
                if (!std::iswspace(line_chars[i].c)) {
                    continue;
                }
                size_t j = i;
                while (j < line_chars.size() && std::iswspace(line_chars[j].c)) {
                    j++;
                }
                if (i > 0 && j < line_chars.size() &&
                    line_chars[i - 1].link_id == line_chars[j].link_id) {
                    const auto& a = line_chars[i - 1];
                    const auto& b = line_chars[j];
                    for (size_t k = i; k < j; k++) {
                        line_chars[k].bold = a.bold && b.bold;
                        line_chars[k].italic = a.italic && b.italic;
                        line_chars[k].monospace = a.monospace && b.monospace;
                        line_chars[k].strikeout = a.strikeout && b.strikeout;
                    }
                }
                i = j;
            }

            // Suppress isolated 1-character italic runs (almost always inline math variables).
            for (size_t i = 0; i < line_chars.size(); ) {
                if (!line_chars[i].italic || std::iswspace(line_chars[i].c)) {
                    i++;
                    continue;
                }
                size_t j = i;
                size_t non_space_count = 0;
                while (j < line_chars.size() && line_chars[j].italic) {
                    if (!std::iswspace(line_chars[j].c)) {
                        non_space_count++;
                    }
                    j++;
                }
                if (non_space_count == 1 && !line_chars[i].bold && !line_chars[i].monospace) {
                    for (size_t k = i; k < j; k++) {
                        line_chars[k].italic = false;
                    }
                }
                i = j;
            }
        }

        std::wstring line_out;
        if (logical_lines[li].starts_with_bullet) {
            line_out = L"- ";
        }

        // Emit segments (either a link run or a styled plain-text run).
        size_t idx = 0;
        while (idx < line_chars.size()) {
            if (line_chars[idx].link_id >= 0) {
                int lid = line_chars[idx].link_id;
                size_t end = idx;
                std::wstring desc;
                bool all_bold = config.preserve_styles;
                bool all_italic = config.preserve_styles;
                bool any_non_space = false;
                while (end < line_chars.size() && line_chars[end].link_id == lid) {
                    desc.push_back(line_chars[end].c);
                    if (!std::iswspace(line_chars[end].c)) {
                        any_non_space = true;
                        all_bold = all_bold && line_chars[end].bold;
                        all_italic = all_italic && line_chars[end].italic && !line_chars[end].is_math_font;
                    }
                    end++;
                }
                std::wstring formatted_link = format_org_link(links[static_cast<size_t>(lid)], desc, config);
                wchar_t prev_c = line_out.empty() ? L'\0' : line_out.back();
                wchar_t next_c = (end < line_chars.size()) ? line_chars[end].c : L'\0';
                if (any_non_space && (all_bold || all_italic) &&
                    is_org_pre_border(prev_c) && is_org_post_border(next_c)) {
                    if (all_bold && all_italic) {
                        formatted_link = L"*/" + formatted_link + L"/*";
                    } else if (all_bold) {
                        formatted_link = L"*" + formatted_link + L"*";
                    } else if (all_italic) {
                        formatted_link = L"/" + formatted_link + L"/";
                    }
                }
                line_out += formatted_link;
                idx = end;
            } else {
                StyleKey st = style_of(line_chars[idx], config.preserve_styles);
                size_t end = idx;
                std::wstring run;
                while (end < line_chars.size() &&
                       line_chars[end].link_id < 0 &&
                       style_of(line_chars[end], config.preserve_styles) == st) {
                    run.push_back(line_chars[end].c);
                    end++;
                }
                wchar_t prev_c = line_out.empty() ? L'\0' : line_out.back();
                wchar_t next_c = (end < line_chars.size())
                    ? ((line_chars[end].link_id >= 0) ? L'[' : line_chars[end].c)
                    : L'\0';
                line_out += wrap_styled_text(run, st, prev_c, next_c);
                idx = end;
            }
        }

        output += line_out;
    }

    return output;
}
