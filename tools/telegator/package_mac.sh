#!/bin/bash
# Установщик Telegator для macOS: .dmg с окном «перетащите в Программы».
#
#   tools/telegator/package_mac.sh [Telegator.app]
#
# Берёт Release-сборку (по умолчанию ~/Projects/telegator-wt/dev/out/Release),
# убирает отладочные символы, кладёт в пакет настройки панели и подписывает.
# Настройки панели — ~/Projects/telegator-wt/package.json (вне git: адрес
# сервера и аккаунты — бизнес-специфика), только раздел "panel"; ключ журнала
# у каждого компьютера свой и в пакет не попадает. Подпись — ad hoc, либо
# сертификат Developer ID из TELEGATOR_SIGN_IDENTITY. Результат —
# ~/Projects/telegator-wt/dist/Telegator-<версия>-<коммит>.dmg.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=${TELEGATOR_ROOT:-$HOME/Projects/telegator-wt}
app=${1:-$root/dev/out/Release/Telegator.app}
config=${TELEGATOR_CONFIG:-$root/package.json}
venv=${TELEGATOR_VENV:-$root/.package-venv}
dist=${TELEGATOR_DIST:-$root/dist}
python=${TELEGATOR_PYTHON:-/opt/homebrew/bin/python3.12}

[ -d "$app" ] || { echo "Нет сборки: $app" >&2; exit 1; }
[ -f "$config" ] || { echo "Нет настроек панели: $config" >&2; exit 1; }
"$python" - "$config" <<'PY'
import json
import math
import sys

def reject():
    sys.exit("package.json: invalid panel configuration")

try:
    with open(sys.argv[1], encoding="utf-8") as stream:
        def unique_object(pairs):
            result = {}
            for key, value in pairs:
                if key in result:
                    reject()
                result[key] = value
            return result

        data = json.load(stream, object_pairs_hook=unique_object)
except (OSError, UnicodeError, json.JSONDecodeError):
    reject()

if not isinstance(data, dict) or set(data) != {"panel"}:
    reject()
panel = data["panel"]
if not isinstance(panel, dict) or set(panel) != {"url", "accounts"}:
    reject()
if not isinstance(panel["url"], str) or not panel["url"].strip():
    reject()
accounts = panel["accounts"]
if not isinstance(accounts, list) or not accounts:
    reject()
for account in accounts:
    if isinstance(account, bool) or not isinstance(account, (int, float)):
        reject()
    if isinstance(account, float):
        if not math.isfinite(account) or not account.is_integer() or account <= 0:
            reject()
    elif account <= 0:
        reject()
    if account > 9223372036854775807:
        reject()
PY

if [ ! -x "$venv/bin/dmgbuild" ]; then
	"$python" -m venv "$venv"
	"$venv/bin/pip" -q install dmgbuild Pillow
fi

version=$(plutil -extract CFBundleShortVersionString raw "$app/Contents/Info.plist")
commit=${TELEGATOR_COMMIT:-$(git -C "$(dirname "$app")/../.." rev-parse --short HEAD 2>/dev/null || echo local)}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

ditto "$app" "$work/Telegator.app"
xattr -cr "$work/Telegator.app"
strip -S -x "$work/Telegator.app/Contents/MacOS/Telegator"
cp "$config" "$work/Telegator.app/Contents/Resources/telegator.json"
if [ -n "${TELEGATOR_SIGN_IDENTITY:-}" ]; then
	codesign --force --deep --options runtime --timestamp -s "$TELEGATOR_SIGN_IDENTITY" "$work/Telegator.app"
else
	codesign --force --deep -s - "$work/Telegator.app"
fi
codesign --verify --deep --strict "$work/Telegator.app"

"$venv/bin/python" "$here/dmg/background.py" "$work/background.png"
mkdir -p "$dist"
out="$dist/Telegator-$version-$commit.dmg"
rm -f "$out"
"$venv/bin/dmgbuild" -s "$here/dmg/settings.py" \
	-D app="$work/Telegator.app" -D background="$work/background.png" \
	"Telegator" "$out" >/dev/null
echo "$out ($(du -h "$out" | cut -f1 | tr -d ' '))"
