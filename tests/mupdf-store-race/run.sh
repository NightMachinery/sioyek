#!/usr/bin/env bash
# Build mupdf with AddressSanitizer, build the stress program against it,
# and run it. See docs/mupdf-patches.md.
#
# Usage: tests/mupdf-store-race/run.sh [--unpatched] [THREADS] [SECONDS]
#
#   --unpatched  temporarily reverse patches/mupdf/0001-*.patch, build into
#                mupdf/build/sanitize-unpatched, and restore the patch on exit.
#                Use it to see the crash the patch fixes.
#
# The mupdf libraries go to mupdf/build/sanitize (or sanitize-unpatched),
# never to mupdf/build/release, so this does not disturb the app build.
# Everything else is written to tests/mupdf-store-race/build/.
set -euo pipefail

test_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${test_dir}/../.." && pwd)"
mupdf_dir="${repo_root}/mupdf"
patch="${repo_root}/patches/mupdf/0001-pdf-store-do-not-compare-keys-across-documents.patch"
out_dir="${test_dir}/build"

variant=sanitize
if [[ "${1:-}" == --unpatched ]]; then
    variant=sanitize-unpatched
    shift
fi
threads="${1:-4}"
seconds="${2:-20}"

"${repo_root}/scripts/apply_mupdf_patches.sh"
if [[ "${variant}" == sanitize-unpatched ]]; then
    git -C "${mupdf_dir}" apply --reverse "${patch}"
    trap '"${repo_root}/scripts/apply_mupdf_patches.sh"' EXIT
    echo "run.sh: patch reversed for this run; it is re-applied on exit"
fi

if [[ "$(uname)" == Darwin ]]; then
    lib_path="${LIBRARY_PATH:-/opt/homebrew/lib}"
    jobs="$(sysctl -n hw.ncpu)"
else
    lib_path="${LIBRARY_PATH:-/usr/lib}"
    jobs="$(nproc)"
fi

# Apple clang has no LeakSanitizer, which mupdf's sanitize build also asks
# for, so override SANITIZE_FLAGS with AddressSanitizer alone.
make -C "${mupdf_dir}" build=sanitize OUT="build/${variant}" \
    SANITIZE_FLAGS=-fsanitize=address HAVE_GLUT=no HAVE_X11=no \
    XLIBS="-L${lib_path}" -j"${jobs}" libs

mkdir -p "${out_dir}"
python3 -I "${test_dir}/make_pdf.py" "${out_dir}/inline-shadings.pdf"
cc -g -O1 -fsanitize=address -fno-omit-frame-pointer \
    -I"${mupdf_dir}/include" "${test_dir}/stress.c" \
    "${mupdf_dir}/build/${variant}/libmupdf.a" \
    "${mupdf_dir}/build/${variant}/libmupdf-third.a" \
    -lpthread -lm -o "${out_dir}/stress-${variant}"

ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=0:halt_on_error=1}" \
    "${out_dir}/stress-${variant}" "${out_dir}/inline-shadings.pdf" "${threads}" "${seconds}"
