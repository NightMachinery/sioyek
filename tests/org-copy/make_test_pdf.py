#!/usr/bin/env python3
"""Generate a minimal 2-page PDF with internal /GoTo links, external /URI link,
and bold/italic/monospace fonts for testing org_copy against real MuPDF extraction."""

import sys


def build_pdf() -> bytes:
    objects: list[bytes] = []

    def add_obj(body: bytes) -> int:
        objects.append(body)
        return len(objects)

    # 1: Catalog (placeholder, updated below)
    # 2: Pages (placeholder, updated below)
    # 3-6: Standard PDF Type1 fonts
    # 7: Page 1
    # 8: Page 1 content stream
    # 9: Page 2
    # 10: Page 2 content stream
    # 11-13: Link annotations on Page 1

    catalog_id = add_obj(b"<< /Type /Catalog /Pages 2 0 R >>")
    pages_id = add_obj(b"<< /Type /Pages /Kids [7 0 R 9 0 R] /Count 2 >>")
    assert catalog_id == 1 and pages_id == 2

    add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")          # 3: F1
    add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>")     # 4: F2
    add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Oblique >>")  # 5: F3
    add_obj(b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")            # 6: F4

    # Page 1
    add_obj(
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        b"/Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R /F4 6 0 R >> >> "
        b"/Contents 8 0 R /Annots [11 0 R 12 0 R 13 0 R] >>"
    )  # 7

    # Put links at explicit absolute coordinates on line 1 (y=700),
    # and styled text in a separate block below (y=620).
    p1_stream = (
        b"BT\n"
        b"/F1 12 Tf\n"
        b"1 0 0 1 72 700 Tm (See) Tj\n"
        b"1 0 0 1 110 700 Tm (Section 2) Tj\n"
        b"1 0 0 1 180 700 Tm (and) Tj\n"
        b"1 0 0 1 215 700 Tm (Slide 2) Tj\n"
        b"1 0 0 1 270 700 Tm (or) Tj\n"
        b"1 0 0 1 295 700 Tm (Example Docs) Tj\n"
        b"ET\n"
        b"BT\n"
        b"/F2 12 Tf 1 0 0 1 72 620 Tm (Bold) Tj\n"
        b"/F1 12 Tf 1 0 0 1 110 620 Tm (and) Tj\n"
        b"/F3 12 Tf 1 0 0 1 142 620 Tm (italic) Tj\n"
        b"/F1 12 Tf 1 0 0 1 182 620 Tm (with) Tj\n"
        b"/F4 12 Tf 1 0 0 1 216 620 Tm (--page) Tj\n"
        b"ET\n"
    )
    add_obj(
        f"<< /Length {len(p1_stream)} >>\nstream\n".encode()
        + p1_stream
        + b"endstream"
    )  # 8

    # Page 2
    add_obj(
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        b"/Resources << /Font << /F1 3 0 R >> >> /Contents 10 0 R >>"
    )  # 9

    p2_stream = b"BT /F1 12 Tf 1 0 0 1 72 500 Tm (Target Section 2) Tj ET\n"
    add_obj(
        f"<< /Length {len(p2_stream)} >>\nstream\n".encode()
        + p2_stream
        + b"endstream"
    )  # 10

    # 11: Internal link "Section 2" -> Page 2 XYZ (72, 500) => MuPDF y = 792 - 500 = 292
    add_obj(
        b"<< /Type /Annot /Subtype /Link /Rect [108 695 170 714] /Border [0 0 0] "
        b"/A << /S /GoTo /D [9 0 R /XYZ 72 500 null] >> >>"
    )
    # 12: Internal link "Slide 2" -> Page 2 Fit (whole page)
    add_obj(
        b"<< /Type /Annot /Subtype /Link /Rect [213 695 260 714] /Border [0 0 0] "
        b"/A << /S /GoTo /D [9 0 R /Fit] >> >>"
    )
    # 13: External link "Example Docs" -> https://example.org/docs
    add_obj(
        b"<< /Type /Annot /Subtype /Link /Rect [293 695 385 714] /Border [0 0 0] "
        b"/A << /S /URI /URI (https://example.org/docs) >> >>"
    )

    out = bytearray(b"%PDF-1.4\n")
    offsets = [0]
    for idx, body in enumerate(objects, start=1):
        offsets.append(len(out))
        out += f"{idx} 0 obj\n".encode()
        out += body
        out += b"\nendobj\n"

    xref_pos = len(out)
    out += f"xref\n0 {len(objects) + 1}\n".encode()
    out += b"0000000000 65535 f \n"
    for off in offsets[1:]:
        out += f"{off:010d} 00000 n \n".encode()
    out += (
        f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\n"
        f"startxref\n{xref_pos}\n%%EOF\n"
    ).encode()
    return bytes(out)


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} OUTPUT.pdf")
    with open(sys.argv[1], "wb") as f:
        f.write(build_pdf())


if __name__ == "__main__":
    main()
