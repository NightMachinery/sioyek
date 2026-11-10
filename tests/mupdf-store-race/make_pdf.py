#!/usr/bin/env python3
"""Write a one-page PDF whose shadings are all direct (inline) dictionaries.

The page /Resources is a direct dictionary in the page object. Its /Shading
entry holds NUM_SHADINGS axial shadings, each a direct dictionary with a
direct /Function dictionary. The page content paints each shading several
times with the `sh` operator, once inside a form XObject as well, so that
pdf_load_shading looks up many non-indirect keys in the shared fz_store.
Those lookups take the linear-scan path of fz_find_item, which compares the
key against every other non-indirect key in the store.

Usage: make_pdf.py OUT.pdf
"""

import sys

NUM_SHADINGS = 48
PAINTS_PER_SHADING = 4


def shading_dict(i):
    r = (i % 8) / 7.0
    g = ((i // 8) % 6) / 5.0
    return (
        "<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [0 0 10 0] "
        "/Extend [true true] "
        "/Function << /FunctionType 2 /Domain [0 1] "
        f"/C0 [{r:.3f} {g:.3f} 0] /C1 [0 0 1] /N 1 >> >>"
    )


def main():
    out_path = sys.argv[1]
    shadings = " ".join(f"/Sh{i} {shading_dict(i)}" for i in range(NUM_SHADINGS))

    ops = []
    for k in range(PAINTS_PER_SHADING):
        for i in range(NUM_SHADINGS):
            x = (i % 8) * 12
            y = (i // 8) * 12 + k * 80
            ops.append(f"q 1 0 0 1 {x} {y} cm 0 0 10 10 re W n /Sh{i} sh Q")
    ops.append("q 1 0 0 1 0 400 cm /Fm0 Do Q")
    content = "\n".join(ops).encode("ascii")

    form_ops = "\n".join(
        f"q 1 0 0 1 {(i % 8) * 12} {(i // 8) * 12} cm 0 0 10 10 re W n /Sh{i} sh Q"
        for i in range(NUM_SHADINGS)
    ).encode("ascii")

    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        (
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 500] "
            f"/Resources << /Shading << {shadings} >> "
            "/XObject << /Fm0 5 0 R >> >> /Contents 4 0 R >>"
        ).encode("ascii"),
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
        (
            "<< /Type /XObject /Subtype /Form /BBox [0 0 100 100] "
            f"/Resources << /Shading << {shadings} >> >> "
            f"/Length {len(form_ops)} >>\nstream\n"
        ).encode("ascii")
        + form_ops
        + b"\nendstream",
    ]

    data = bytearray(b"%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for num, body in enumerate(objs, start=1):
        offsets.append(len(data))
        data += b"%d 0 obj\n" % num + body + b"\nendobj\n"
    xref_at = len(data)
    data += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for off in offsets:
        data += b"%010d 00000 n \n" % off
    data += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (
        len(objs) + 1,
        xref_at,
    )
    with open(out_path, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    main()
