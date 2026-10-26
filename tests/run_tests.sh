#!/usr/bin/env bash
# =============================================================================
#  run_tests.sh  --  MiniLang compiler test suite
#
#  Usage (from the project root, in an MSYS2 UCRT64 shell):
#      bash tests/run_tests.sh [path/to/minilang.exe]
#      bash tests/run_tests.sh bin/minilang.exe --update   # regenerate golden files
#
#  Every tests/<phase>_tests/NAME.ml is a test.  What is checked depends on
#  which companion files exist next to it:
#
#    NAME.tokens    stdout of `minilang NAME.ml --tokens` must match exactly
#    NAME.ast       stdout of `minilang NAME.ml --ast` must match exactly
#    NAME.optsrc    the optimized program printed by --optimized-ast must match
#    NAME.expected  the program is compiled and run; its output must match,
#                   both with and without the optimizer (-O0), and the
#                   generated .ll file must pass LLVM's verifier (opt/llvm-as)
#    NAME.err       compilation must FAIL (exit code 1) and every line of
#                   NAME.err must appear somewhere in the error output
# =============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COMPILER="${1:-$ROOT/bin/minilang.exe}"
UPDATE=0
[ "${2:-}" = "--update" ] && UPDATE=1

if [ ! -x "$COMPILER" ]; then
    echo "error: compiler '$COMPILER' not found -- build it first (cmake --build build or make)"
    exit 2
fi

WORK="$ROOT/output/tests"
mkdir -p "$WORK"

# LLVM verifier: prefer `opt -passes=verify`, fall back to llvm-as.
if command -v opt >/dev/null 2>&1; then
    verify_ir() { opt -passes=verify -disable-output "$1" 2>&1; }
elif command -v llvm-as >/dev/null 2>&1; then
    verify_ir() { llvm-as "$1" -o /dev/null 2>&1; }
else
    verify_ir() { return 0; }
    echo "note: opt/llvm-as not found; skipping IR verification"
fi

pass=0
fail=0
failed_names=()

ok()   { pass=$((pass + 1)); printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
bad()  { fail=$((fail + 1)); failed_names+=("$1"); printf '  \033[31mFAIL\033[0m  %s\n' "$1"; [ -n "${2:-}" ] && printf '%s\n' "$2" | sed 's/^/          /'; }

# Compare actual output with a golden file (or rewrite it in --update mode).
check_golden() {   # name golden actual
    if [ $UPDATE -eq 1 ]; then printf '%s\n' "$3" > "$2"; ok "$1 (updated)"; return; fi
    local expected; expected="$(cat "$2")"
    if [ "$expected" == "$3" ]; then ok "$1"
    elif command -v diff >/dev/null 2>&1; then
        bad "$1" "$(diff <(printf '%s\n' "$expected") <(printf '%s\n' "$3") | head -20)"
    else
        bad "$1" "--- expected ---"$'\n'"$expected"$'\n'"--- actual ---"$'\n'"$3"
    fi
}

for dir in lexer_tests parser_tests semantic_tests optimization_tests codegen_tests; do
    echo ""
    echo "== $dir"
    for src in "$ROOT/tests/$dir"/*.ml; do
        [ -e "$src" ] || continue
        base="${src%.ml}"
        name="$dir/$(basename "$base")"
        rel="tests/$dir/$(basename "$src")"
        ll="$WORK/$(basename "$base").ll"
        tested=0
        cd "$ROOT"

        if [ -f "$base.tokens" ]; then
            tested=1
            out="$("$COMPILER" "$rel" --tokens -o "$ll" 2>/dev/null)"
            out="$(printf '%s\n' "$out" | sed '/^=*$/d;/^  TOKENS$/d')"
            check_golden "$name [tokens]" "$base.tokens" "$out"
        fi

        if [ -f "$base.ast" ]; then
            tested=1
            out="$("$COMPILER" "$rel" --ast -o "$ll" 2>/dev/null)"
            out="$(printf '%s\n' "$out" | sed '/^=*$/d;/^  ABSTRACT SYNTAX TREE$/d')"
            check_golden "$name [ast]" "$base.ast" "$out"
        fi

        if [ -f "$base.optsrc" ]; then
            tested=1
            out="$("$COMPILER" "$rel" --optimized-ast -o "$ll" 2>/dev/null | sed -n '/as MiniLang source/,$p' | tail -n +2)"
            check_golden "$name [optimized]" "$base.optsrc" "$out"
        fi

        if [ -f "$base.expected" ]; then
            tested=1
            for mode in "" "-O0"; do
                label="$name [run${mode:+ $mode}]"
                # Windows programs print CRLF line endings; compare text only.
                out="$("$COMPILER" "$rel" --run $mode -o "$ll" 2>"$WORK/stderr.txt" | tr -d "\r"; exit "${PIPESTATUS[0]}")"
                status=$?
                if [ $status -ne 0 ]; then bad "$label" "exit status $status: $(cat "$WORK/stderr.txt")"; continue; fi
                if ! v="$(verify_ir "$ll")"; then bad "$label" "LLVM verifier rejected IR: $v"; continue; fi
                if [ -z "$mode" ]; then check_golden "$label" "$base.expected" "$out"
                elif [ "$(cat "$base.expected")" == "$out" ]; then ok "$label"
                else bad "$label" "output differs from expected without optimization"; fi
            done
        fi

        if [ -f "$base.err" ]; then
            tested=1
            "$COMPILER" "$rel" -o "$ll" >/dev/null 2>"$WORK/stderr.txt"
            status=$?
            errors="$(cat "$WORK/stderr.txt")"
            if [ $status -ne 1 ]; then
                bad "$name [error]" "expected exit status 1, got $status"
            else
                missing=""
                while IFS= read -r line || [ -n "$line" ]; do
                    [ -z "$line" ] && continue
                    grep -qF -- "$line" "$WORK/stderr.txt" || missing+="missing: $line"$'\n'
                done < "$base.err"
                if [ -z "$missing" ]; then ok "$name [error]"
                else bad "$name [error]" "$missing--- actual output ---"$'\n'"$errors"; fi
            fi
        fi

        [ $tested -eq 0 ] && echo "  SKIP  $name (no .tokens/.ast/.optsrc/.expected/.err file)"
    done
done

echo ""
echo "============================================================"
echo "  $pass passed, $fail failed"
if [ $fail -gt 0 ]; then
    printf '  failed: %s\n' "${failed_names[@]}"
    exit 1
fi
echo "============================================================"
exit 0
