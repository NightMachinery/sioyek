#include "org_copy.h"

#include <mupdf/fitz.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::wstring utf8_to_wide(const std::string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<wchar_t>(c));
    }
    return out;
}

std::string wide_to_ascii(const std::wstring& s) {
    std::string out;
    for (wchar_t c : s) {
        if (c >= 0 && c < 128) {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned int>(c));
            out += buf;
        }
    }
    return out;
}

bool rects_overlap(fz_rect a, fz_rect b) {
    return !(a.x1 < b.x0 || b.x1 < a.x0 || a.y1 < b.y0 || b.y1 < a.y0);
}

std::wstring extract_page_as_org(
    fz_context* ctx,
    fz_document* doc,
    const std::wstring& virtual_path,
    int page_num,
    bool presentation_mode,
    const OrgCopyConfig& config) {

    fz_page* page = fz_load_page(ctx, doc, page_num);
    fz_link* raw_links = fz_load_links(ctx, page);

    struct SimpleLink {
        fz_rect rect;
        std::string uri;
    };
    std::vector<SimpleLink> page_links;
    for (fz_link* l = raw_links; l != nullptr; l = l->next) {
        if (l->uri) {
            page_links.push_back({l->rect, l->uri});
        }
    }

    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;
    fz_stext_page* stext = fz_new_stext_page_from_page_number(ctx, doc, page_num, &opts);

    std::vector<OrgStyledChar> chars;
    std::vector<OrgLinkTarget> targets;
    std::unordered_map<int, int> link_map;

    int block_id = 0;
    int line_id = 0;
    for (fz_stext_block* b = stext->first_block; b; b = b->next) {
        if (b->type != FZ_STEXT_BLOCK_TEXT) continue;
        int bid = block_id++;
        for (fz_stext_line* ln = b->u.t.first_line; ln; ln = ln->next) {
            int lid = line_id++;
            for (fz_stext_char* chr = ln->first_char; chr; chr = chr->next) {
                OrgStyledChar sc;
                sc.c = static_cast<wchar_t>(chr->c);
                sc.bold = ((chr->flags & FZ_STEXT_BOLD) != 0) ||
                          (chr->font && fz_font_is_bold(ctx, chr->font) != 0);
                sc.italic = (chr->font && fz_font_is_italic(ctx, chr->font) != 0);
                sc.monospace = (chr->font && fz_font_is_monospaced(ctx, chr->font) != 0);
                sc.strikeout = (chr->flags & FZ_STEXT_STRIKEOUT) != 0;
                if (chr->font) {
                    const char* fname = fz_font_name(ctx, chr->font);
                    if (fname) sc.is_math_font = is_likely_math_font_name(fname);
                }
                sc.block_id = bid;
                sc.line_id = lid;
                sc.line_end = (chr->next == nullptr);

                fz_rect cr = fz_rect_from_quad(chr->quad);
                float cy = (cr.y0 + cr.y1) / 2.0f;
                float cx = (cr.x0 + cr.x1) / 2.0f;
                float h = cr.y1 - cr.y0;
                cr.x0 = cx;
                cr.x1 = cx;
                cr.y0 = cy - h / 5.0f;
                cr.y1 = cy + h / 5.0f;

                int matched = -1;
                for (size_t i = 0; i < page_links.size(); i++) {
                    if (rects_overlap(cr, page_links[i].rect)) {
                        matched = static_cast<int>(i);
                        break;
                    }
                }
                if (matched >= 0) {
                    auto it = link_map.find(matched);
                    if (it == link_map.end()) {
                        OrgLinkTarget t;
                        const std::string& uri = page_links[static_cast<size_t>(matched)].uri;
                        if (fz_is_external_link(ctx, uri.c_str())) {
                            t.kind = OrgLinkTarget::Kind::External;
                            t.raw_uri = utf8_to_wide(uri);
                        } else {
                            fz_link_dest dest = fz_resolve_link_dest(ctx, doc, uri.c_str());
                            int tp = fz_page_number_from_location(ctx, doc, dest.loc);
                            if (tp >= 0) {
                                fz_page* target_page = fz_load_page(ctx, doc, tp);
                                fz_rect tb = fz_bound_page(ctx, target_page);
                                float pw = tb.x1 - tb.x0;
                                fz_drop_page(ctx, target_page);

                                bool is_whole =
                                    dest.type == FZ_LINK_DEST_FIT ||
                                    dest.type == FZ_LINK_DEST_FIT_B ||
                                    dest.type == FZ_LINK_DEST_FIT_V ||
                                    dest.type == FZ_LINK_DEST_FIT_BV;
                                float dy = std::isnan(dest.y) ? 0.0f : dest.y;

                                t.kind = OrgLinkTarget::Kind::Internal;
                                t.file_path = virtual_path;
                                t.target_page_1based = tp + 1;
                                t.target_x = pw / 2.0f;
                                t.target_y = dy;
                                t.whole_page = presentation_mode || is_whole || (dy <= 0.5f);
                                t.current_zoom = 2.0f;
                            }
                        }
                        int new_id = static_cast<int>(targets.size());
                        targets.push_back(t);
                        link_map[matched] = new_id;
                        sc.link_id = new_id;
                    } else {
                        sc.link_id = it->second;
                    }
                }
                chars.push_back(sc);
            }
        }
    }

    fz_drop_stext_page(ctx, stext);
    fz_drop_link(ctx, raw_links);
    fz_drop_page(ctx, page);

    return serialize_selection_to_org(chars, targets, config);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " TEST.pdf\n";
        return 2;
    }

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    fz_document* doc = fz_open_document(ctx, argv[1]);

    OrgCopyConfig cfg;
    cfg.home_dir = L"/Users/alice";
    cfg.path_abbrevs = L"~nt=~/notes";
    cfg.preserve_styles = true;
    cfg.include_zoom_in_internal_links = false;

    const std::wstring vpath = L"/Users/alice/notes/sample.pdf";

    // Normal mode: XYZ (72, 500) => MuPDF y=292, center_x=306; Fit => page-only ::2; URI => external
    std::wstring got = extract_page_as_org(ctx, doc, vpath, 0, false, cfg);
    std::wstring expected =
        L"See [[sioyek-v1:~nt/sample.pdf::2:306:292][Section 2]] and "
        L"[[sioyek-v1:~nt/sample.pdf::2][Slide 2]] or "
        L"[[https://example.org/docs][Example Docs]]\n\n"
        L"*Bold* and /italic/ with ~--page~";

    if (got != expected) {
        std::cerr << "FAIL [mupdf normal mode]\n"
                  << "  expected: \"" << wide_to_ascii(expected) << "\"\n"
                  << "  actual:   \"" << wide_to_ascii(got) << "\"\n";
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return 1;
    }

    // Presentation mode: even XYZ link collapses to whole-slide ::2
    std::wstring got_pres = extract_page_as_org(ctx, doc, vpath, 0, true, cfg);
    std::wstring expected_pres =
        L"See [[sioyek-v1:~nt/sample.pdf::2][Section 2]] and "
        L"[[sioyek-v1:~nt/sample.pdf::2][Slide 2]] or "
        L"[[https://example.org/docs][Example Docs]]\n\n"
        L"*Bold* and /italic/ with ~--page~";

    if (got_pres != expected_pres) {
        std::cerr << "FAIL [mupdf presentation mode]\n"
                  << "  expected: \"" << wide_to_ascii(expected_pres) << "\"\n"
                  << "  actual:   \"" << wide_to_ascii(got_pres) << "\"\n";
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return 1;
    }

    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    std::cout << "mupdf org-copy integration tests passed (2 checks)\n";
    return 0;
}
