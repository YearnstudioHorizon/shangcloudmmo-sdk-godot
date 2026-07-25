#!/usr/bin/env bash
# Assemble build artifacts into per-platform and all-in-one release zips.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
STAGING="${ROOT}/.release-staging"
DIST="${ROOT}/dist"

rm -rf "${STAGING}" "${DIST}"
mkdir -p "${STAGING}/bin" "${DIST}"

# CI downloads into artifacts/<platform>/ with one of:
#   artifacts/windows/windows/*.dll          (upload path=bin/)
#   artifacts/windows/bin/windows/*.dll
#   artifacts/windows/*.dll                  (flat)
copy_platform() {
  local platform="$1"
  local src="${ROOT}/artifacts/${platform}"
  local dest="${STAGING}/bin/${platform}"

  if [[ ! -d "${src}" ]]; then
    echo "error: missing artifact dir: ${src}" >&2
    return 1
  fi

  mkdir -p "${dest}"

  if [[ -d "${src}/bin/${platform}" ]]; then
    cp -a "${src}/bin/${platform}/." "${dest}/"
  elif [[ -d "${src}/${platform}" ]]; then
    cp -a "${src}/${platform}/." "${dest}/"
  else
    # Copy shared libs / frameworks from anywhere under src
    while IFS= read -r -d '' f; do
      cp -a "${f}" "${dest}/"
    done < <(find "${src}" -type f \( \
      -name '*.dll' -o -name '*.so' -o -name '*.dylib' -o \
      -name 'libshangcloud_mmo*' \
    \) -print0)

    while IFS= read -r -d '' d; do
      cp -a "${d}" "${dest}/"
    done < <(find "${src}" -type d -name '*.framework' -print0)
  fi

  local count
  count="$(find "${dest}" \( -type f -o -type d -name '*.framework' \) | wc -l | tr -d ' ')"
  if [[ "${count}" -eq 0 ]]; then
    echo "error: no binaries found for platform=${platform} under ${src}" >&2
    find "${src}" -maxdepth 5 -print >&2 || true
    return 1
  fi

  echo "packed platform=${platform} (${count} entries):"
  find "${dest}" -maxdepth 2 -print
}

copy_platform windows
copy_platform linux
copy_platform macos

cp "${ROOT}/shangcloud_mmo.gdextension" "${STAGING}/shangcloud_mmo.gdextension"

for platform in windows linux macos; do
  case "${platform}" in
    windows) zip_name="shangcloud-mmo-windows-x86_64.zip" ;;
    linux)   zip_name="shangcloud-mmo-linux-x86_64.zip" ;;
    macos)   zip_name="shangcloud-mmo-macos-universal.zip" ;;
  esac
  (
    cd "${STAGING}"
    zip -ry "${DIST}/${zip_name}" \
      "shangcloud_mmo.gdextension" \
      "bin/${platform}"
  )
done

(
  cd "${STAGING}"
  zip -ry "${DIST}/shangcloud-mmo-all.zip" \
    "shangcloud_mmo.gdextension" \
    "bin"
)

echo "Release packages:"
ls -lah "${DIST}"
