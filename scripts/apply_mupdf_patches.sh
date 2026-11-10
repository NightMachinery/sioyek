#!/usr/bin/env bash
# Apply every patches/mupdf/*.patch to the mupdf submodule, idempotently.
# A patch that is already applied is skipped; a patch that neither applies
# nor reverses cleanly stops the build. See docs/mupdf-patches.md.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mupdf_dir="${repo_root}/mupdf"
patch_dir="${repo_root}/patches/mupdf"

if [[ ! -e "${mupdf_dir}/.git" ]]; then
    echo "apply_mupdf_patches: ${mupdf_dir} is not a git checkout (run: git submodule update --init mupdf)" >&2
    exit 1
fi

shopt -s nullglob
patches=("${patch_dir}"/*.patch)
shopt -u nullglob

for patch in "${patches[@]}"; do
    name="$(basename "${patch}")"
    if git -C "${mupdf_dir}" apply --reverse --check "${patch}" 2>/dev/null; then
        echo "apply_mupdf_patches: already applied: ${name}"
    elif git -C "${mupdf_dir}" apply --check "${patch}" 2>/dev/null; then
        git -C "${mupdf_dir}" apply "${patch}"
        echo "apply_mupdf_patches: applied: ${name}"
    else
        echo "apply_mupdf_patches: ${name} neither applies nor is already applied; the mupdf checkout has diverged. Refresh the patch." >&2
        git -C "${mupdf_dir}" apply --check "${patch}" || true
        exit 1
    fi
done
