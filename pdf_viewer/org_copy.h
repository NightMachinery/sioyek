#pragma once

#include <optional>
#include <string>
#include <vector>

struct OrgCopyConfig {
    bool preserve_styles = true;
    bool include_zoom_in_internal_links = false;
    bool add_newlines = false;
    bool fallback_to_current_location = false;
    std::wstring path_abbrevs;
    std::wstring home_dir;
};

struct OrgLinkTarget {
    enum class Kind {
        None,
        Internal,
        External,
        FilePdf,
        FileOther
    };

    Kind kind = Kind::None;
    std::wstring raw_uri;
    std::wstring file_path;
    int target_page_1based = 0;
    float target_x = 0.0f;
    float target_y = 0.0f;
    bool whole_page = true;
    float current_zoom = 0.0f;
};

struct OrgStyledChar {
    wchar_t c = L'\0';
    bool bold = false;
    bool italic = false;
    bool monospace = false;
    bool strikeout = false;
    bool is_math_font = false;
    int link_id = -1;
    int block_id = 0;
    int line_id = 0;
    bool line_end = false;
};

std::wstring abbreviate_org_path(
    const std::wstring& path,
    const std::wstring& home_dir,
    const std::wstring& abbrevs_spec);

std::wstring escape_org_link_target(const std::wstring& target);
std::wstring escape_org_link_description(const std::wstring& desc);

std::wstring format_sioyek_v1_link(
    const std::wstring& file_path,
    int page_1based,
    std::optional<float> x,
    std::optional<float> y,
    std::optional<float> zoom,
    const std::wstring& home_dir,
    const std::wstring& abbrevs_spec,
    const std::wstring& description = L"");

std::wstring format_org_link(
    const OrgLinkTarget& target,
    const std::wstring& description,
    const OrgCopyConfig& config);

bool is_likely_math_font_name(const std::string& font_name);

std::wstring serialize_selection_to_org(
    const std::vector<OrgStyledChar>& chars,
    const std::vector<OrgLinkTarget>& links,
    const OrgCopyConfig& config);
