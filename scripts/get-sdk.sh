#!/usr/bin/env bash
set -euo pipefail

version="0.2.0-rc1"
asset="lingtts-sdk-${version}-community.tar.gz"
repo="https://github.com/LingCoreX-AI/LingTTS/releases/download/v${version}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${1:-${root}/.downloads}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$dest"

curl --fail --location --retry 3 --output "${tmp}/${asset}" "${repo}/${asset}"
curl --fail --location --retry 3 --output "${tmp}/${asset}.sha256" "${repo}/${asset}.sha256"
(cd "$tmp" && sha256sum --check "${asset}.sha256")
tar -xzf "${tmp}/${asset}" -C "$dest"
printf '\nSDK ready: %s\n' "${dest}/lingtts-sdk-${version}-community"
printf 'Build from that directory with:\n  . ~/esp/esp-idf-v5.5.6/export.sh\n  idf.py -C examples/hello -B build/hello build\n'
