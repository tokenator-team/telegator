#!/usr/bin/env bash
set -euo pipefail

repo=$(cd "$(dirname "$0")/../../.." && pwd)
package_script=$repo/tools/telegator/package_mac.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

real_python=$(command -v python3)
mkdir -p "$work/bin" "$work/app/Contents/MacOS" \
    "$work/app/Contents/Resources" "$work/venv/bin"
touch "$work/app/Contents/MacOS/Telegator"
printf 'valid fixture\n' >"$work/config.json"
printf 'not used\n' >"$work/app/Contents/Resources/telegator.json"

cat >"$work/bin/plutil" <<'MOCK'
#!/usr/bin/env bash
printf '9.8.7\n'
MOCK
cat >"$work/bin/git" <<'MOCK'
#!/usr/bin/env bash
printf '0123456\n'
MOCK
cat >"$work/bin/ditto" <<'MOCK'
#!/usr/bin/env bash
cp -R "$1" "$2"
MOCK
for command in xattr strip; do
    cat >"$work/bin/$command" <<'MOCK'
#!/usr/bin/env bash
exit 0
MOCK
done
cat >"$work/bin/codesign" <<'MOCK'
#!/usr/bin/env bash
app=${@: -1}
embedded=$app/Contents/Resources/telegator.json
if [[ ${1:-} == --verify ]]; then
    [[ -f "$embedded" ]] || exit 21
else
    cmp -s "$embedded" "$TELEGATOR_CONFIG" || exit 22
    cp "$embedded" "$TELEGATOR_EMBEDDED_CAPTURE"
fi
MOCK
cat >"$work/bin/python" <<'MOCK'
#!/usr/bin/env bash
exec "$REAL_PYTHON" "$@"
MOCK
cat >"$work/venv/bin/python" <<'MOCK'
#!/usr/bin/env bash
printf 'background\n' >"${@: -1}"
MOCK
cat >"$work/venv/bin/dmgbuild" <<'MOCK'
#!/usr/bin/env bash
for last; do :; done
touch "$last"
MOCK
for mock in "$work"/bin/* "$work"/venv/bin/*; do
    chmod +x "$mock"
done
touch "$work/dist-placeholder"
mkdir -p "$work/dist"

run_package() {
    PATH="$work/bin:$PATH" \
    REAL_PYTHON="$real_python" \
    TELEGATOR_ROOT="$work/root" \
    TELEGATOR_CONFIG="$work/config.json" \
    TELEGATOR_DIST="$work/dist" \
    TELEGATOR_VENV="$work/venv" \
    TELEGATOR_PYTHON="$work/bin/python" \
    TELEGATOR_COMMIT="${1:-}" \
    TELEGATOR_EMBEDDED_CAPTURE="$work/embedded.json" \
        bash "$package_script" "$work/app"
}

cat >"$work/config.json" <<'JSON'
{"panel":{"url":"https://panel.example/path","accounts":[1,987654321]}}
JSON
run_package >"$work/valid.out" 2>&1
cmp -s "$work/config.json" "$work/embedded.json"
[[ -f "$work/dist/Telegator-9.8.7-0123456.dmg" ]]
full_sha=0123456789abcdef0123456789abcdef01234567
run_package "$full_sha" >"$work/full-sha.out" 2>&1
[[ -f "$work/dist/Telegator-9.8.7-$full_sha.dmg" ]]

for fixture in journal extra-field wrong-root wrong-panel wrong-url wrong-accounts account-wrong-type account-zero account-fraction; do
    case $fixture in
        journal) json='{"panel":{"url":"https://panel.invalid/secret","accounts":[7]},"journal":{"key":"DO_NOT_PRINT_SECRET"}}' ;;
        extra-field) json='{"panel":{"url":"https://panel.invalid/secret","accounts":[7],"extra":"DO_NOT_PRINT_SECRET"}}' ;;
        wrong-root) json='["DO_NOT_PRINT_SECRET"]' ;;
        wrong-panel) json='{"panel":"DO_NOT_PRINT_SECRET"}' ;;
        wrong-url) json='{"panel":{"url":false,"accounts":[7]}}' ;;
        wrong-accounts) json='{"panel":{"url":"https://panel.invalid/secret","accounts":"DO_NOT_PRINT_SECRET"}}' ;;
        account-wrong-type) json='{"panel":{"url":"https://panel.invalid/secret","accounts":["DO_NOT_PRINT_SECRET"]}}' ;;
        account-zero) json='{"panel":{"url":"https://panel.invalid/secret","accounts":[0]}}' ;;
        account-fraction) json='{"panel":{"url":"https://panel.invalid/secret","accounts":[1.5]}}' ;;
    esac
    printf '%s\n' "$json" >"$work/config.json"
    if run_package >"$work/rejected.out" 2>&1; then
        printf 'Expected package_mac.sh to reject fixture: %s\n' "$fixture" >&2
        exit 1
    fi
    if grep -q 'DO_NOT_PRINT_SECRET\|panel.invalid' "$work/rejected.out"; then
        printf 'Rejected fixture leaked configuration values: %s\n' "$fixture" >&2
        exit 1
    fi
done

printf 'OK package_mac: valid config embedded; invalid fixtures rejected without values.\n'
