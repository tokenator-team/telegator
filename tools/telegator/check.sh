#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

for script in tools/telegator/*.sh; do
    bash -n "$script"
    printf 'OK bash: %s\n' "$script"
done

workflows=(.github/workflows/telegator-*.yml)
if command -v python3 >/dev/null && python3 -c 'import yaml' 2>/dev/null; then
    python3 - "${workflows[@]}" <<'PY'
import sys
import yaml

for path in sys.argv[1:]:
    with open(path, encoding="utf-8") as stream:
        list(yaml.safe_load_all(stream))
    print(f"OK YAML (python3): {path}")
PY
elif command -v ruby >/dev/null && ruby -ryaml -e '' 2>/dev/null; then
    ruby -ryaml -e 'ARGV.each do |path|
        YAML.parse_stream(File.read(path))
        puts "OK YAML (ruby): #{path}"
    end' "${workflows[@]}"
else
    printf 'ERROR: YAML check requires python3 with yaml or ruby with yaml.\n' >&2
    exit 1
fi

requisites_test="${TELEGATOR_REQUISITES_TEST:-out/Debug/test_telegator_requisites}"
requisites_vectors="${TELEGATOR_REQUISITES_VECTORS:-$HOME/.tokenator-konveyer/rekvizity-telegator/vectors.json}"
if [[ ! -x "$requisites_test" ]]; then
    printf 'SKIP requisites: no executable %s (no build performed).\n' "$requisites_test"
elif [[ ! -f "$requisites_vectors" ]]; then
    printf 'SKIP requisites: no private vectors %s.\n' "$requisites_vectors"
else
    "$requisites_test" "$requisites_vectors"
fi

printf 'OK: Telegator checks completed (without building the client).\n'
