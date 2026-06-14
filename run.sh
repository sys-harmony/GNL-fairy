#!/bin/bash

set -u

RED="\033[0;31m"
GREEN="\033[0;32m"
PINK='\033[0;95m'
YELLOW="\033[0;33m"
RESET="\033[0m"

VERBOSE=0
if [[ "${1:-}" == "-v" || "${1:-}" == "--verbose" ]]; then
    VERBOSE=1
fi

TESTER_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( cd "$TESTER_DIR/.." && pwd )"
TMP_DIR="/tmp/gnl_fairy_$$"

mkdir -p "$TMP_DIR"

cd "$PROJECT_DIR" || exit 1

TESTER_NAME=$(basename "$TESTER_DIR")

BUFFER_SIZES="1 42 9999 10000000"
VG_SIZES="1 42 9999"

# ---- result accumulators ----------------------------------------------------
NORM_RES=0
PROTO_RES=0
SRC_RES=0
DEFAULT_RES=0
EXTERN_ERRORS=""
GLOBALS_ERRORS=""
BUILD_RES=0
RUN_RES=0
BONUS_VERSION=0
BONUS_EXTERN_ERRORS=""
BONUS_GLOBALS_ERRORS=""
BONUS_BUILD_RES=0
BONUS_RUN_RES=0

# ---- alignment helpers ------------------------------------------------------
# \033[44G jumps the cursor to absolute column 34 on the current line,
# regardless of label length or emoji width — no more tab arithmetic.
COL=37
ok()   { printf "\033[${COL}G  Done\n"; }
fail() { printf "\033[${COL}G${RED}Failed${RESET}\n"; }

cleanup() {
    echo ""
    printf "🧹 Cleaning up..."
    ok
    echo ""
}

print_result() {
    echo ""
    if [[ $NORM_RES -eq 0 && $PROTO_RES -eq 0 && $SRC_RES -eq 0 && $DEFAULT_RES -eq 0 \
        && -z "$EXTERN_ERRORS" && -z "$GLOBALS_ERRORS" && $BUILD_RES -eq 0 && $RUN_RES -eq 0 \
        && -z "$BONUS_EXTERN_ERRORS" && -z "$BONUS_GLOBALS_ERRORS" \
        && $BONUS_BUILD_RES -eq 0 && $BONUS_RUN_RES -eq 0 ]]; then
        echo -e "${GREEN}╔════════════════════════════════════════╗${RESET}"
        echo -e "${GREEN}║           OH MY, YOU PASSED!           ║${RESET}"
        echo -e "${GREEN}╚════════════════════════════════════════╝${RESET}"
    else
        echo -e "${RED}╔════════════════════════════════════════╗${RESET}"
        echo -e "${RED}║          OH NO... YOU FAILED!          ║${RESET}"
        echo -e "${RED}╚════════════════════════════════════════╝${RESET}"
    fi
}

trap 'print_result; cleanup' EXIT INT TERM

echo ""
echo -e "${PINK}╔════════════════════════════════════════╗${RESET}"
echo -e "${PINK}║              GNL-FAIRY 🧚              ║${RESET}"
echo -e "${PINK}╚════════════════════════════════════════╝${RESET}"
echo

# ---- norm -------------------------------------------------------------------
printf "📝 Checking norm..."
NORM_OUTPUT=$(find . -type d -name "$TESTER_NAME" -prune -o \
    \( -name "*.c" -o -name "*.h" \) -type f -print | xargs -r norminette 2>&1)
if echo "$NORM_OUTPUT" | grep -q "Error"; then
    NORM_RES=1
    fail
    echo ""
    echo "$NORM_OUTPUT" | grep "Error"
    echo ""
else
    ok
fi

# ---- version ----------------------------------------------------------------
printf "🔖 Checking version..."
if [ -f "get_next_line_bonus.c" ]; then
    BONUS_VERSION=1
    printf "\033[38GBonus\n"
else
    printf "\033[34G${YELLOW}Mandatory${RESET}\n"
fi

# ---- locate & verify mandatory files -----------------------------------------
printf "📂 Checking files..."

# Ensure all mandatory files are exactly at the root of the repository
if [ ! -f "get_next_line.c" ] || [ ! -f "get_next_line_utils.c" ] || [ ! -f "get_next_line.h" ]; then
    SRC_RES=1
    fail
    echo ""
    echo "Missing one or more mandatory files at the root of the repository:"
    [ ! -f "get_next_line.c" ] && echo "- get_next_line.c"
    [ ! -f "get_next_line_utils.c" ] && echo "- get_next_line_utils.c"
    [ ! -f "get_next_line.h" ] && echo "- get_next_line.h"
    echo ""
    exit 1
fi
MAND_SRCS="get_next_line.c get_next_line_utils.c"
ok

# ---- prototype --------------------------------------------------------------
printf "📋 Checking prototype..."
PROTO_REGEX='(^|[^_[:alnum:]])char[[:space:]]*\*[[:space:]]*get_next_line[[:space:]]*'
PROTO_REGEX+='\([[:space:]]*int[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)?[[:space:]]*\)'

# Look for the exact prototype strictly inside get_next_line.h
if ! grep -qP "$PROTO_REGEX" "get_next_line.h"; then
    PROTO_RES=1
    fail
    echo ""
    echo -e "Missing or malformed prototype in get_next_line.h, expected:\nchar\t*get_next_line(int fd)"
    echo ""
else
    ok
fi

# The include directory is simply the current directory (.)
IDIRS="-I."

# ---- default BUFFER_SIZE (compiles without -D) ------------------------------
printf "🎛️  Checking default BUFFER_SIZE..."
if cc -Wall -Wextra -Werror $IDIRS -c "get_next_line.c" -o "$TMP_DIR/gnl.o" 2> "$TMP_DIR/def.err" \
    && cc -Wall -Wextra -Werror $IDIRS -c "get_next_line_utils.c" -o "$TMP_DIR/gnl_utils.o" 2>> "$TMP_DIR/def.err"; then
    ok
else
    DEFAULT_RES=1
    fail
    echo ""
    echo "Project must compile WITHOUT -D BUFFER_SIZE (provide a default):"
    echo -e "$(cat "$TMP_DIR/def.err")"
    echo ""
    exit 1
fi

# ---- externals & globals ----------------------------------------------------
EXTERN_ALLOWED="read malloc free"
DEFINED_SYMS=""
collect_defined_syms() {
    DEFINED_SYMS=" $(for o in "$@"; do
        [[ -f "$o" ]] && nm --defined-only "$o" 2>/dev/null | awk '{print $NF}'
    done | sort -u | tr '\n' ' ') "
}
check_obj() {
    local obj=$1
    local forbidden=""
    for ext in $(nm -u "$obj" 2>/dev/null | awk '{print $2}'); do
        [[ "$ext" == ft_* || "$ext" == __* ]] && continue
        [[ " $EXTERN_ALLOWED " =~ " $ext " ]] && continue
        [[ "$DEFINED_SYMS" == *" $ext "* ]] && continue
        forbidden="$forbidden $ext"
    done
    [[ -n "$forbidden" ]] && echo "$(basename "$obj" .o): forbidden:$forbidden"
}
check_globals() {
    local obj=$1
    local g
    g=$(nm "$obj" 2>/dev/null | awk '$2 ~ /^[BCDGS]$/ {print $3}' | tr '\n' ' ')
    [[ -n "$g" ]] && echo "$(basename "$obj" .o): global(s):$g"
}

printf "🔍 Checking externals..."
collect_defined_syms "$TMP_DIR/gnl.o" "$TMP_DIR/gnl_utils.o"
for obj in "$TMP_DIR/gnl.o" "$TMP_DIR/gnl_utils.o"; do
    result=$(check_obj "$obj")
    [[ -n "$result" ]] && EXTERN_ERRORS="$EXTERN_ERRORS$result\n"
done
if [[ -z "$EXTERN_ERRORS" ]]; then
    ok
else
    fail
    echo ""
    echo -e "$EXTERN_ERRORS"
fi

printf "🌍 Checking globals..."
for obj in "$TMP_DIR/gnl.o" "$TMP_DIR/gnl_utils.o"; do
    result=$(check_globals "$obj")
    [[ -n "$result" ]] && GLOBALS_ERRORS="$GLOBALS_ERRORS$result\n"
done
if [[ -z "$GLOBALS_ERRORS" ]]; then
    ok
else
    fail
    echo ""
    echo -e "Global variables are forbidden:\n$GLOBALS_ERRORS"
fi

# ---- build + run a full suite (mandatory or bonus) --------------------------
run_suite() {
    local pfx=$1 basic=$2 leak=$3 buildvar=$4 runvar=$5
    shift 5
    local srcs="$*"
    local tag=${pfx// /_}
    local bs bin rc logs="" build_fail=0 run_fail=0

    printf "🔨 Building ${pfx}tests..."
    for bs in $BUFFER_SIZES; do
        bin="$TMP_DIR/${tag}basic_bs${bs}"
        cc -Wall -Wextra -Wno-unused-result -DVERBOSE=$VERBOSE -DBUFFER_SIZE=$bs $IDIRS \
            "$TESTER_NAME/$basic" "$TESTER_NAME/utils.c" $srcs \
            -Wl,--wrap=malloc,--wrap=read -o "$bin" 2> "$bin.err" \
            || { build_fail=1; logs+="[$basic BUFFER_SIZE=$bs]\n$(cat "$bin.err")\n"; }
    done
    for bs in $VG_SIZES; do
        bin="$TMP_DIR/${tag}leak_bs${bs}"
        cc -Wall -Wextra -Wno-unused-result -DBUFFER_SIZE=$bs $IDIRS \
            "$TESTER_NAME/$leak" "$TESTER_NAME/utils.c" $srcs \
            -Wl,--wrap=malloc,--wrap=read -o "$bin" 2> "$bin.err" \
            || { build_fail=1; logs+="[$leak BUFFER_SIZE=$bs]\n$(cat "$bin.err")\n"; }
    done
    if [[ $build_fail -ne 0 ]]; then
        eval "$buildvar=1"
        fail
        echo -e "$logs"
        return
    fi
    ok

    printf "🧪 Running ${pfx}tests..."
    logs=""
    for bs in $BUFFER_SIZES; do
        bin="$TMP_DIR/${tag}basic_bs${bs}"
        timeout 60 "$bin" > "$bin.log" 2>&1
        rc=$?
        [[ $rc -ne 0 ]] && run_fail=1
        if [[ $rc -ne 0 || $VERBOSE -eq 1 ]]; then
            logs+="\n${YELLOW}── functional (BUFFER_SIZE=$bs) ──${RESET}\n$(cat "$bin.log")\n"
        fi
        [[ $rc -eq 124 ]] && logs+="${RED}(timed out)${RESET}\n"
    done
    for bs in $VG_SIZES; do
        bin="$TMP_DIR/${tag}leak_bs${bs}"
        timeout 120 valgrind --leak-check=full --show-leak-kinds=all \
            --errors-for-leak-kinds=all --error-exitcode=42 --track-origins=yes \
            "$bin" >/dev/null 2>"$bin.vglog"
        rc=$?
        [[ $rc -ne 0 ]] && run_fail=1
        if [[ $rc -ne 0 || $VERBOSE -eq 1 ]]; then
            logs+="\n${YELLOW}── valgrind (BUFFER_SIZE=$bs) ──${RESET}\n$(cat "$bin.vglog")\n"
        fi
    done
    if [[ $run_fail -ne 0 ]]; then
        eval "$runvar=1"
        fail
    else
        ok
    fi
    [[ -n "$logs" ]] && echo -e "$logs"
}

run_suite "" "basic_tests.c" "leak_tests.c" BUILD_RES RUN_RES "$MAND_SRCS"

# ---- bonus ------------------------------------------------------------------
if [[ $BONUS_VERSION -eq 1 ]]; then
    
    printf "📂 Checking bonus files..."
    # Ensure all required bonus files are exactly at the root of the repository
    if [ ! -f "get_next_line_bonus.c" ] || [ ! -f "get_next_line_utils_bonus.c" ] || [ ! -f "get_next_line_bonus.h" ]; then
        BONUS_BUILD_RES=1
        fail
        echo ""
        echo "Missing one or more bonus files at the root of the repository:"
        [ ! -f "get_next_line_bonus.c" ] && echo "- get_next_line_bonus.c"
        [ ! -f "get_next_line_utils_bonus.c" ] && echo "- get_next_line_utils_bonus.c"
        [ ! -f "get_next_line_bonus.h" ] && echo "- get_next_line_bonus.h"
        exit 1
    fi
    ok

    # ---- bonus prototype ----------------------------------------------------
    printf "📋 Checking bonus prototype..."
    
    # Look for the exact prototype strictly inside get_next_line_bonus.h
    if ! grep -qP "$PROTO_REGEX" "get_next_line_bonus.h"; then
        BONUS_BUILD_RES=1
        fail
        echo ""
        echo -e "Missing or malformed prototype in get_next_line_bonus.h, expected:\nchar\t*get_next_line(int fd)"
        echo ""
    else
        ok
    fi

    # ---- bonus default BUFFER_SIZE (compiles without -D) --------------------
    printf "🎛️  Checking bonus BUFFER_SIZE..."
    if cc -Wall -Wextra -Werror $IDIRS -c "get_next_line_bonus.c" -o "$TMP_DIR/gnlb.o" 2> "$TMP_DIR/bonus_def.err" \
        && cc -Wall -Wextra -Werror $IDIRS -c "get_next_line_utils_bonus.c" -o "$TMP_DIR/gnlb_utils.o" 2>> "$TMP_DIR/bonus_def.err"; then
        ok
    else
        BONUS_BUILD_RES=1
        fail
        echo ""
        echo "Bonus project must compile WITHOUT -D BUFFER_SIZE (provide a default):"
        echo -e "$(cat "$TMP_DIR/bonus_def.err")"
        echo ""
        exit 1
    fi

    printf "🔍 Checking bonus externals..."
    collect_defined_syms "$TMP_DIR/gnlb.o" "$TMP_DIR/gnlb_utils.o"
    for obj in "$TMP_DIR/gnlb.o" "$TMP_DIR/gnlb_utils.o"; do
        [[ -f "$obj" ]] || continue
        result=$(check_obj "$obj")
        [[ -n "$result" ]] && BONUS_EXTERN_ERRORS="$BONUS_EXTERN_ERRORS$result\n"
    done
    if [[ -z "$BONUS_EXTERN_ERRORS" ]]; then
        ok
    else
        fail
        echo ""
        echo -e "$BONUS_EXTERN_ERRORS"
    fi

    printf "🌍 Checking bonus globals..."
    for obj in "$TMP_DIR/gnlb.o" "$TMP_DIR/gnlb_utils.o"; do
        [[ -f "$obj" ]] || continue
        result=$(check_globals "$obj")
        [[ -n "$result" ]] && BONUS_GLOBALS_ERRORS="$BONUS_GLOBALS_ERRORS$result\n"
    done
    if [[ -z "$BONUS_GLOBALS_ERRORS" ]]; then
        ok
    else
        fail
        echo ""
        echo -e "Global variables are forbidden:\n$BONUS_GLOBALS_ERRORS"
    fi

    run_suite "bonus mandatory " "basic_tests.c" "leak_tests.c" \
        BONUS_BUILD_RES BONUS_RUN_RES "get_next_line_bonus.c get_next_line_utils_bonus.c"
    run_suite "bonus " "basic_tests_bonus.c" "leak_tests_bonus.c" \
        BONUS_BUILD_RES BONUS_RUN_RES "get_next_line_bonus.c get_next_line_utils_bonus.c"
fi

exit 0
