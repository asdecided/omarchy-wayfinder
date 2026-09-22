#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
workspace=$(mktemp -d)
trap 'rm -rf -- "$workspace"' EXIT
mkdir -p "$workspace/prefix/bin" "$workspace/prefix/share/wayfinder/omarchy" "$workspace/bin" "$workspace/home/.config/wayfinder"
cp "$root/scripts/wayfinder-omarchy" "$workspace/prefix/bin/"
cp "$root"/companion/* "$workspace/prefix/share/wayfinder/omarchy/"
export HOME="$workspace/home" PATH="$workspace/bin:$PATH" TEST_LOG="$workspace/calls"
helper="$workspace/prefix/bin/wayfinder-omarchy"
source_dir="$workspace/prefix/share/wayfinder/omarchy"
plugins="$HOME/.config/omarchy/plugins"
target="$plugins/io.github.asdecided.wayfinder"
printf 'custom policy\n' > "$HOME/.config/wayfinder/policy"
printf 'old Router\n' > "$HOME/router"
cat > "$workspace/bin/omarchy-shell" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" >> "$TEST_LOG"
[[ ${TEST_OFFLINE:-0} == 0 ]] || exit 1
case "$2" in
  listPlugins) echo '[{"id":"io.github.asdecided.wayfinder","enabled":true}]' ;;
  rescanPlugins) echo '' ;;
  enablePlugin|setPluginEnabled) echo "${TEST_ENABLE_RESULT:-ok}" ;;
  *) exit 99 ;;
esac
SH
cat > "$workspace/bin/omarchy-plugin-validate" <<'SH'
#!/usr/bin/env bash
[[ ${TEST_INVALID:-0} == 0 ]] && jq -e '.id == "io.github.asdecided.wayfinder"' "$1/manifest.json" >/dev/null
SH
chmod +x "$workspace/bin/"* "$helper"

if TEST_OFFLINE=1 "$helper" enable > "$workspace/output"; then exit 1; fi
[[ ! -e $target ]]
if TEST_INVALID=1 "$helper" enable > "$workspace/output"; then exit 1; fi
[[ ! -e $target ]]
"$helper" enable
[[ -L $target && $(readlink "$target") == "$source_dir" ]]
"$helper" enable
[[ $(find "$plugins" -maxdepth 1 -name '.wayfinder-before-app.*' | wc -l) == 0 ]]
# A package update is immediately visible through the link; there is no copy
# or Git checkout that can become stale or independently update itself.
printf 'updated package\n' > "$source_dir/version-test"
[[ $(cat "$target/version-test") == 'updated package' ]]
if TEST_ENABLE_RESULT=unknown "$helper" disable > "$workspace/output"; then exit 1; fi
[[ -L $target ]]
"$helper" disable
[[ ! -L $target && -f $source_dir/manifest.json ]]

# Preserve a dirty legacy checkout in full, including untracked files.
mkdir -p "$target/.git"
printf 'local edits\n' > "$target/custom"
"$helper" enable > "$workspace/output"
backup=$(find "$plugins" -maxdepth 1 -type d -name '.wayfinder-before-app.*' | head -1)
[[ -d $backup/plugin/.git && $(cat "$backup/plugin/custom") == 'local edits' ]]
[[ -L $target ]]
"$helper" disable

# Refuse to delete an unrelated checkout; preserve unrelated symlink targets.
mkdir -p "$target"
printf 'keep\n' > "$target/custom"
if "$helper" disable > "$workspace/output"; then exit 1; fi
[[ -f $target/custom ]]
mv "$target" "$workspace/external"
ln -s "$workspace/external" "$target"
"$helper" enable
[[ $(cat "$workspace/external/custom") == keep ]]
[[ $(find "$plugins" -path '*/plugin' -type l | wc -l) == 1 ]]
"$helper" disable

# Failure after registration reports failure and leaves a retryable link.
if TEST_ENABLE_RESULT=unknown "$helper" enable > "$workspace/output"; then exit 1; fi
[[ -L $target ]]
"$helper" enable
"$helper" disable
[[ $(cat "$HOME/.config/wayfinder/policy") == 'custom policy' ]]
[[ $(cat "$HOME/router") == 'old Router' ]]
! grep -Eq 'service|router|sudo|--section' "$TEST_LOG"
echo 'companion: fresh install, retry, migration, ownership, update and removal passed'
