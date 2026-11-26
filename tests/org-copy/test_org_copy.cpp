#include "org_copy.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;

std::string to_utf8_ascii(const std::wstring& s) {
    std::string out;
    for (wchar_t c : s) {
        if (c >= 0 && c < 128) {
            out.push_back(static_cast<char>(c));
        } else {
            out += "\\u";
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned int>(c));
            out += buf;
        }
    }
    return out;
}

void expect_eq(const std::wstring& actual, const std::wstring& expected, const char* name) {
    if (actual != expected) {
        std::cerr << "FAIL [" << name << "]\n"
                  << "  expected: \"" << to_utf8_ascii(expected) << "\"\n"
                  << "  actual:   \"" << to_utf8_ascii(actual) << "\"\n";
        std::exit(1);
    }
    g_passed++;
}

void append_str(
    std::vector<OrgStyledChar>& out,
    const std::wstring& text,
    int block_id,
    int line_id,
    int link_id = -1,
    bool bold = false,
    bool italic = false,
    bool monospace = false,
    bool strikeout = false,
    bool is_math_font = false,
    bool mark_line_end = false) {
    for (size_t i = 0; i < text.size(); i++) {
        OrgStyledChar sc;
        sc.c = text[i];
        sc.block_id = block_id;
        sc.line_id = line_id;
        sc.link_id = link_id;
        sc.bold = bold;
        sc.italic = italic;
        sc.monospace = monospace;
        sc.strikeout = strikeout;
        sc.is_math_font = is_math_font;
        sc.line_end = mark_line_end && (i + 1 == text.size());
        out.push_back(sc);
    }
}

void test_path_abbreviation() {
    const std::wstring home = L"/Users/alice";
    const std::wstring abbrevs = L"~nt=~/notes; ~nt_priv=~/notes/private; ~bk=/Volumes/books/";

    // Longest prefix wins
    expect_eq(
        abbreviate_org_path(L"/Users/alice/notes/private/paper.pdf", home, abbrevs),
        L"~nt_priv/paper.pdf",
        "path_abbrev_longest_prefix");

    // Prefix boundary respected (notes2 must not match ~/notes)
    expect_eq(
        abbreviate_org_path(L"/Users/alice/notes2/paper.pdf", home, abbrevs),
        L"~/notes2/paper.pdf",
        "path_abbrev_boundary");

    // Standard ~nt rule
    expect_eq(
        abbreviate_org_path(L"/Users/alice/notes/slides/deck.pdf", home, abbrevs),
        L"~nt/slides/deck.pdf",
        "path_abbrev_custom_rule");

    // External volume rule with trailing slash trimmed
    expect_eq(
        abbreviate_org_path(L"/Volumes/books/cs/intro.pdf", home, abbrevs),
        L"~bk/cs/intro.pdf",
        "path_abbrev_absolute_rule");

    // Fallback to $HOME -> ~
    expect_eq(
        abbreviate_org_path(L"/Users/alice/papers/demo.pdf", home, L""),
        L"~/papers/demo.pdf",
        "path_abbrev_home_fallback");

    // Unrelated absolute path unchanged
    expect_eq(
        abbreviate_org_path(L"/opt/docs/manual.pdf", home, abbrevs),
        L"/opt/docs/manual.pdf",
        "path_abbrev_unrelated");
}

void test_escaping_and_sioyek_v1() {
    expect_eq(
        escape_org_link_target(L"sioyek-v1:~/dir [1]/file [2].pdf::3"),
        L"sioyek-v1:~/dir \\[1\\]/file \\[2\\].pdf::3",
        "escape_link_target");

    expect_eq(
        escape_org_link_description(L"Citation [12]\nnext"),
        L"Citation {12} next",
        "escape_link_description");

    expect_eq(
        format_sioyek_v1_link(
            L"/Users/alice/notes/slides/talk.pdf",
            22,
            181.417f,
            138.625f,
            3.80882f,
            L"/Users/alice",
            L"~nt=~/notes"),
        L"[[sioyek-v1:~nt/slides/talk.pdf::22:181.417:138.625:3.80882]]",
        "format_sioyek_v1_full_location");

    expect_eq(
        format_sioyek_v1_link(
            L"/Users/alice/notes/slides/talk.pdf",
            5,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            L"/Users/alice",
            L"~nt=~/notes",
            L"Slide 5"),
        L"[[sioyek-v1:~nt/slides/talk.pdf::5][Slide 5]]",
        "format_sioyek_v1_page_only_with_desc");
}

void test_internal_and_external_links() {
    OrgCopyConfig cfg;
    cfg.home_dir = L"/Users/alice";
    cfg.path_abbrevs = L"~nt=~/notes";
    cfg.preserve_styles = true;
    cfg.include_zoom_in_internal_links = false;

    std::vector<OrgLinkTarget> links;

    OrgLinkTarget internal_mid;
    internal_mid.kind = OrgLinkTarget::Kind::Internal;
    internal_mid.file_path = L"/Users/alice/notes/paper.pdf";
    internal_mid.target_page_1based = 7;
    internal_mid.target_x = 306.0f;
    internal_mid.target_y = 240.5f;
    internal_mid.whole_page = false;
    internal_mid.current_zoom = 2.5f;
    links.push_back(internal_mid);

    OrgLinkTarget internal_slide;
    internal_slide.kind = OrgLinkTarget::Kind::Internal;
    internal_slide.file_path = L"/Users/alice/notes/paper.pdf";
    internal_slide.target_page_1based = 12;
    internal_slide.whole_page = true;
    links.push_back(internal_slide);

    OrgLinkTarget ext;
    ext.kind = OrgLinkTarget::Kind::External;
    ext.raw_uri = L"https://example.org/docs";
    links.push_back(ext);

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"See ", 0, 0);
    append_str(chars, L"Figure 2", 0, 0, 0);
    append_str(chars, L" and ", 0, 0);
    append_str(chars, L"[12]", 0, 0, 1);
    append_str(chars, L" or ", 0, 0);
    append_str(chars, L"project page", 0, 0, 2, false, false, false, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, links, cfg),
        L"See [[sioyek-v1:~nt/paper.pdf::7:306:240.5][Figure 2]] and [[sioyek-v1:~nt/paper.pdf::12][{12}]] or [[https://example.org/docs][project page]]",
        "internal_and_external_links_smart");

    // With include_zoom enabled
    cfg.include_zoom_in_internal_links = true;
    expect_eq(
        serialize_selection_to_org(chars, links, cfg),
        L"See [[sioyek-v1:~nt/paper.pdf::7:306:240.5:2.5][Figure 2]] and [[sioyek-v1:~nt/paper.pdf::12][{12}]] or [[https://example.org/docs][project page]]",
        "internal_links_with_zoom");
}

void test_wrapped_multiline_link_and_dehyphenation() {
    OrgCopyConfig cfg;
    cfg.home_dir = L"/Users/alice";

    OrgLinkTarget ref;
    ref.kind = OrgLinkTarget::Kind::Internal;
    ref.file_path = L"/Users/alice/paper.pdf";
    ref.target_page_1based = 15;
    ref.whole_page = true;

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"In ", 0, 0);
    append_str(chars, L"Smith et", 0, 0, 0, false, false, false, false, false, true);
    append_str(chars, L"al. (2024)", 0, 1, 0);
    append_str(chars, L" we study repre-", 0, 1, -1, false, false, false, false, false, true);
    append_str(chars, L"sentation.", 0, 2, -1, false, false, false, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, {ref}, cfg),
        L"In [[sioyek-v1:~/paper.pdf::15][Smith et al. (2024)]] we study representation.",
        "wrapped_link_and_dehyphenation");
}

void test_inline_styles_and_math_guards() {
    OrgCopyConfig cfg;
    cfg.preserve_styles = true;

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"The ", 0, 0);
    append_str(chars, L"key idea", 0, 0, -1, true, false, false, false, false);
    append_str(chars, L" uses ", 0, 0);
    append_str(chars, L"x", 0, 0, -1, false, true, false, false, true); // math font
    append_str(chars, L" and ", 0, 0);
    append_str(chars, L"n", 0, 0, -1, false, true, false, false, false); // single-char italic guard
    append_str(chars, L" with ", 0, 0);
    append_str(chars, L"emphasis", 0, 0, -1, false, true, false, false, false);
    append_str(chars, L" and ", 0, 0);
    append_str(chars, L"--page", 0, 0, -1, false, false, true, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, {}, cfg),
        L"The *key idea* uses x and n with /emphasis/ and ~--page~",
        "inline_styles_and_math_guards");

    // Preserving styles off -> plain prose
    cfg.preserve_styles = false;
    expect_eq(
        serialize_selection_to_org(chars, {}, cfg),
        L"The key idea uses x and n with emphasis and --page",
        "preserve_styles_disabled");

    // Math font name detection
    assert(is_likely_math_font_name("ABCDEF+CMMI10"));
    assert(is_likely_math_font_name("LatinModernMath-Regular"));
    assert(is_likely_math_font_name("Symbol"));
    assert(!is_likely_math_font_name("TimesNewRomanPS-ItalicMT"));
    assert(!is_likely_math_font_name("CMTI10"));
    g_passed++;
}

void test_paragraphs_and_bullets() {
    OrgCopyConfig cfg;
    cfg.preserve_styles = true;

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"Intro paragraph.", 0, 0, -1, false, false, false, false, false, true);
    append_str(chars, L"\u2022 First bullet", 1, 1, -1, false, false, false, false, false, true);
    append_str(chars, L"continued here.", 1, 2, -1, false, false, false, false, false, true);
    append_str(chars, L"\u2022 Second bullet.", 2, 3, -1, false, false, false, false, false, true);
    append_str(chars, L"Closing paragraph.", 3, 4, -1, false, false, false, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, {}, cfg),
        L"Intro paragraph.\n\n- First bullet continued here.\n- Second bullet.\n\nClosing paragraph.",
        "paragraphs_and_bullets");
}

void test_link_space_trimming_and_same_url_bare_link() {
    OrgCopyConfig cfg;
    OrgLinkTarget ext;
    ext.kind = OrgLinkTarget::Kind::External;
    ext.raw_uri = L"https://example.org";

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"Visit", 0, 0);
    append_str(chars, L" https://example.org ", 0, 0, 0);
    append_str(chars, L"now.", 0, 0, -1, false, false, false, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, {ext}, cfg),
        L"Visit [[https://example.org]] now.",
        "link_space_trimming_and_bare_url");
}

void test_cross_file_pdf_and_non_pdf_links() {
    OrgCopyConfig cfg;
    cfg.home_dir = L"/Users/alice";
    cfg.path_abbrevs = L"~nt=~/notes";

    OrgLinkTarget pdf_link;
    pdf_link.kind = OrgLinkTarget::Kind::FilePdf;
    pdf_link.file_path = L"/Users/alice/notes/appendix.pdf";
    pdf_link.target_page_1based = 3;
    pdf_link.whole_page = true;

    OrgLinkTarget txt_link;
    txt_link.kind = OrgLinkTarget::Kind::FileOther;
    txt_link.file_path = L"/Users/alice/notes/code.py";

    std::vector<OrgStyledChar> chars;
    append_str(chars, L"Appendix", 0, 0, 0);
    append_str(chars, L" and ", 0, 0);
    append_str(chars, L"script", 0, 0, 1, false, false, false, false, false, true);

    expect_eq(
        serialize_selection_to_org(chars, {pdf_link, txt_link}, cfg),
        L"[[sioyek-v1:~nt/appendix.pdf::3][Appendix]] and [[file:~nt/code.py][script]]",
        "cross_file_pdf_and_non_pdf");
}

} // namespace

int main() {
    test_path_abbreviation();
    test_escaping_and_sioyek_v1();
    test_internal_and_external_links();
    test_wrapped_multiline_link_and_dehyphenation();
    test_inline_styles_and_math_guards();
    test_paragraphs_and_bullets();
    test_link_space_trimming_and_same_url_bare_link();
    test_cross_file_pdf_and_non_pdf_links();

    std::cout << "org-copy tests passed (" << g_passed << " checks)\n";
    return 0;
}
