#!/bin/bash
# Runs outside the app bundle so replacement cannot remove the running helper.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
stage="$1"
target="$2"
parent_pid="$3"
work="$4"
mode="${5:-relaunch}"
backup="${target%.app}.previous-$(/usr/bin/uuidgen).app"
moved_old=0
installed=0
finish() {
  status=$?
  trap - EXIT
  if [ "$status" -ne 0 ]; then
    if [ "$moved_old" -eq 1 ]; then
      if [ "$installed" -eq 1 ]; then /bin/rm -rf "$target"; fi
      /bin/mv "$backup" "$target" || exit 1
    fi
    if [ "$mode" = relaunch ]; then
      /usr/bin/open "$target" || true
      /usr/bin/osascript - <<'APPLESCRIPT' || true
 display alert "Không thể cài cập nhật Omnikey" message "Bản cũ đã được giữ lại. Xem Library/Logs/Omnikey/updater.log để biết chi tiết." as warning
APPLESCRIPT
    fi
  fi
  /bin/rm -rf "$stage" "$work"
  exit "$status"
}
# Reject unexpected paths before enabling any cleanup or replacement.
[[ "$target" = /*.app && "$stage" = /*.app && "$target" != "$stage" ]] || exit 1
[[ "$(/usr/bin/dirname "$target")" = "$(/usr/bin/dirname "$stage")" ]] || exit 1
[[ "$(/usr/bin/basename "$stage")" = .Omnikey-update-*.app ]] || exit 1
[[ "$work" = /*/OmnikeyUpdate-* && "$parent_pid" =~ ^[0-9]+$ ]] || exit 1
[[ -d "$target" && ! -L "$target" && -d "$stage" && ! -L "$stage" ]] || exit 1
[[ "$mode" = relaunch || "$mode" = no-relaunch ]] || exit 1
trap finish EXIT
# Never replace the bundle while the original application is still running.
for ((i=0; i<60; i++)); do
  if ! /bin/kill -0 "$parent_pid" 2>/dev/null; then break; fi
  /bin/sleep 1
done
if /bin/kill -0 "$parent_pid" 2>/dev/null; then
  echo 'Application did not exit within 60 seconds.' >&2
  exit 1
fi
/usr/bin/codesign --verify --deep --strict "$stage"
/bin/mv "$target" "$backup"
moved_old=1
/bin/mv "$stage" "$target"
installed=1
if [ "$mode" = relaunch ]; then /usr/bin/open "$target"; fi
# Keep the previous bundle until the next successful app launch can be assessed.
echo "Installed: $target"
echo "Previous bundle: $backup"
