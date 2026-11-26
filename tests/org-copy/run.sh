#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${test_dir}/../.." && pwd)"
mupdf_dir="${repo_root}/mupdf"
out_dir="${test_dir}/build"

mkdir -p "${out_dir}"

c++ -std=c++20 -Wall -Wextra -Werror -g -O1 \
    -I"${repo_root}/pdf_viewer" \
    "${repo_root}/pdf_viewer/org_copy.cpp" \
    "${test_dir}/test_org_copy.cpp" \
    -o "${out_dir}/test_org_copy"

"${out_dir}/test_org_copy"

if [[ -f "${mupdf_dir}/build/release/libmupdf.a" ]]; then
    python3 -I "${test_dir}/make_test_pdf.py" "${out_dir}/sample.pdf"
    c++ -std=c++20 -Wall -Wextra -Werror -g -O1 \
        -I"${repo_root}/pdf_viewer" \
        -I"${mupdf_dir}/include" \
        "${repo_root}/pdf_viewer/org_copy.cpp" \
        "${test_dir}/test_mupdf_org_copy.cpp" \
        "${mupdf_dir}/build/release/libmupdf.a" \
        "${mupdf_dir}/build/release/libmupdf-third.a" \
        -lpthread -lm -lz \
        -o "${out_dir}/test_mupdf_org_copy"
    "${out_dir}/test_mupdf_org_copy" "${out_dir}/sample.pdf"
fi
