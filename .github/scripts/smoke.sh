#!/usr/bin/env bash
# (c) Glape Contributors

# quick checks of a built glape binary, usage: smoke.sh <path to glape>
set -euo pipefail

glape="${1:?usage: smoke.sh <path to glape>}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work"

"$glape" version | grep -Eq '^glape [0-9]'
"$glape" help | grep -q 'usage:'
"$glape" help run | grep -q 'glape run'

cat >smoke.glape <<'EOF'
define main():
  print("smoke")
EOF

output="$("$glape" run smoke.glape | tr -d '\r')"
[ "$output" = "smoke" ] || {
    printf 'unexpected output: %s\n' "$output" >&2
    exit 1
}

"$glape" tokens smoke.glape | grep -q 'DEFINE'
"$glape" ast smoke.glape | grep -q '^program$'

if "$glape" no-such-command >/dev/null 2>&1; then
    echo "unknown command was accepted" >&2
    exit 1
fi

printf 'define main(:\n' >bad.glape
if "$glape" run bad.glape >/dev/null 2>&1; then
    echo "a broken script was accepted" >&2
    exit 1
fi

echo "smoke test passed"
