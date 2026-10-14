#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" && ! -L "$1" ]] || { printf 'Usage: verify_lua_artifact.sh artifact\n' >&2; exit 2; }
artifact=$(realpath "$1")
base=${artifact##*/}
[[ "$base" =~ ^lua-ailo-([0-9]+\.[0-9]+\.[0-9]+)(\.tar\.gz|-1\.rockspec|-1\.src\.rock)$ ]] || exit 2
version=${BASH_REMATCH[1]}
name="lua-ailo-$version"
mkdir -p "$root/build/lua-packaging"
work=$(mktemp -d "$root/build/lua-packaging/verify.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
case "$base" in
  *.src.rock)
    printf '%s\n' "$name-1.rockspec" "$name.tar.gz" | LC_ALL=C sort > "$work/expected"
    unzip -Z -1 "$artifact" | LC_ALL=C sort > "$work/actual"
    diff -u "$work/expected" "$work/actual"
    for file in "$name-1.rockspec" "$name.tar.gz"; do
      unzip -p "$artifact" "$file" > "$work/$file"
      "$root/scripts/verify_lua_artifact.sh" "$work/$file"
      if [[ -f "${artifact%/*}/$file" ]]; then cmp "$work/$file" "${artifact%/*}/$file"; fi
    done
    ;;
  *.rockspec)
    sed "s/@VERSION@/$version/g" "$root/lua/lua-ailo.rockspec.in" > "$work/expected"
    cmp "$work/expected" "$artifact"
    luarocks lint "$artifact"
    ;;
  *.tar.gz)
    tar -tzf "$artifact" > "$work/entries"
    [[ -z $(LC_ALL=C sort "$work/entries" | uniq -d) ]] || exit 1
    while IFS= read -r row; do case "${row:0:1}" in -|d) ;; *) exit 1 ;; esac; done < <(tar -tvzf "$artifact")
    while IFS= read -r entry; do
      [[ "$entry" == "$name/"* && ! "$entry" =~ (^|/)\.\.?(/|$) ]] || exit 1
    done < "$work/entries"
    tar -xzf "$artifact" -C "$work"
    payload="$work/$name"
    [[ $(cat "$payload/VERSION") == "$version" ]] || exit 1
    { cat "$root/lua/source-manifest.txt"; printf 'LICENSE\nVERSION\nSOURCE-MANIFEST.sha256\n'; } | LC_ALL=C sort > "$work/expected"
    (cd "$payload" && rg --files --hidden --no-ignore | LC_ALL=C sort) > "$work/actual"
    diff -u "$work/expected" "$work/actual"
    { cat "$root/lua/source-manifest.txt"; printf 'LICENSE\nVERSION\n'; } | LC_ALL=C sort > "$work/ledger-expected"
    : > "$work/ledger-actual"
    while IFS= read -r row; do
      [[ "$row" =~ ^[0-9a-f]{64}[[:space:]][[:space:]](.+)$ ]] || exit 1
      file=${BASH_REMATCH[1]}
      [[ "$file" != /* && ! "$file" =~ (^|/)\.\.?(/|$) ]] || exit 1
      printf '%s\n' "$file" >> "$work/ledger-actual"
    done < "$payload/SOURCE-MANIFEST.sha256"
    LC_ALL=C sort "$work/ledger-actual" -o "$work/ledger-actual"
    diff -u "$work/ledger-expected" "$work/ledger-actual"
    (cd "$payload" && sha256sum --strict -c SOURCE-MANIFEST.sha256 > "$work/checksums")
    status=0
    rg -a -l -F -e "$root" -e "$HOME" -e 'file://' "$payload" || status=$?
    [[ "$status" == 1 ]] || { printf 'lua-verify: local path in source bytes\n' >&2; exit 1; }
    ;;
esac
printf 'Lua final artifact verified: %s\n' "$base"
