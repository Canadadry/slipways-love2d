#!/bin/sh
# usage: end_state.sh normalize <state>
#        end_state.sh check <events> <state> <reference> [<events> <state> <reference>...]
#        end_state.sh baseline <events> <state> <reference> [<events> <state> <reference>...]
# normalize: the game lines of a make state file, sorted, to stdout; exit 1 on
# a `stopped` state or a line kind not in the field table of normalize().
# check: examines every triple, prints `end-state: <events> <reason>` for each
# failure (stopped, no state, no reference, unknown line, differs; the last
# followed by at most 40 differing lines, `-` reference, `+` state); exit 1 if
# any.
# baseline: writes the normalized state to each missing reference, never
# overwrites one; exit 1 if a missing reference cannot be written.

set -u
LC_ALL=C
export LC_ALL

mode=${1:-}
[ $# -gt 0 ] && shift
case $mode in
    normalize)
        if [ $# -ne 1 ]; then
            echo "usage: end_state.sh normalize <state>" >&2
            exit 2
        fi
        ;;
    check|baseline)
        if [ $# -eq 0 ] || [ $(($# % 3)) -ne 0 ]; then
            echo "end-state: arguments come in <events> <state> <reference> triples" >&2
            exit 2
        fi
        ;;
    *)
        echo "usage: end_state.sh normalize <state> | check|baseline <events> <state> <reference>..." >&2
        exit 2
        ;;
esac

# One awk process and no sort or diff: test_end_state runs in make test, where
# every fork counts.
exec awk -v mode="$mode" '
    # The field table: the kept part of a line, "" for a dropped line; sets
    # `stopped`, or appends the line to unknown[1..unknown_count].
    function normalize(line,    f, n) {
        n = split(line, f, " ")
        if (f[1] == "stopped") { stopped = 1; return "" }
        if (f[1] == "clock" && n == 5 && f[2] == "year" && f[4] == "month") return line
        if (f[1] == "difficulty" && n == 2) return line
        if (f[1] == "resources" && n == 3 && (f[2] == "credits" || f[2] == "science" || f[2] == "income")) return line
        if (f[1] == "techs" && n >= 2) return line
        if (f[1] == "camera" && n == 4 && f[2] == "world") return ""
        if (f[1] == "planet" && n == 13 && f[6] == "world" && f[9] == "screen" && f[12] == "visible")
            return f[1] " " f[2] " " f[3] " " f[4] " " f[5] " " f[6] " " f[7] " " f[8]
        if (f[1] == "slipway" && n == 4) return line
        if (f[1] == "structure" && n == 13 && f[4] == "world" && f[7] == "screen" && f[10] == "visible" && f[12] == "links")
            return f[1] " " f[2] " " f[3] " " f[4] " " f[5] " " f[6] " " f[12] " " f[13]
        if (f[1] == "count" && n == 3 && f[2] ~ /^(planets|colonies|slipways|structures|techs|nodes)$/) return line
        if (f[1] == "cheat" && n == 3 && f[2] == "activations") return line
        if (f[1] == "zone" || f[1] == "budget" || f[1] == "cap") return ""
        unknown[++unknown_count] = line
        return ""
    }

    # Reads <path> into lines[1..n] in C order, normalized unless `raw`;
    # returns n, or -1 when the file cannot be opened.
    function read_sorted(path, lines, raw,    line, kept, n, i, status) {
        n = 0
        while ((status = (getline line < path)) > 0) {
            kept = raw ? line : normalize(line)
            if (kept == "") continue
            for (i = ++n; i > 1 && lines[i - 1] "" > kept ""; i--) lines[i] = lines[i - 1]
            lines[i] = kept
        }
        close(path)
        return status < 0 ? -1 : n
    }

    # The `-` and `+` lines of two sorted arrays, at most 40, "" when equal.
    function differences(old, old_n, new, new_n,    i, j, shown, text) {
        i = j = 1
        shown = 0
        text = ""
        while (i <= old_n || j <= new_n) {
            if (j > new_n || (i <= old_n && old[i] "" < new[j] "")) {
                if (++shown <= 40) text = text "-" old[i] "\n"
                i++
            } else if (i > old_n || new[j] "" < old[i] "") {
                if (++shown <= 40) text = text "+" new[j] "\n"
                j++
            } else {
                i++; j++
            }
        }
        return text
    }

    # Prints why the state just read is not clean, to stderr when `to_stderr`;
    # returns 1 when it is not.
    function report_unclean(prefix, to_stderr,    i, text) {
        text = stopped ? prefix "stopped\n" : ""
        for (i = 1; !stopped && i <= unknown_count; i++) text = text prefix "unknown line: " unknown[i] "\n"
        if (to_stderr) printf "%s", text > "/dev/stderr"
        else printf "%s", text
        return text != ""
    }

    BEGIN {
        if (mode == "normalize") {
            n = read_sorted(ARGV[1], state, 0)
            if (n < 0) { print "end-state: no state " ARGV[1] > "/dev/stderr"; exit 1 }
            for (i = 1; i <= n; i++) print state[i]
            exit report_unclean("", 1)
        }
        failed = 0
        replays = 0
        for (arg = 1; arg < ARGC; arg += 3) {
            events = ARGV[arg]; state_path = ARGV[arg + 1]; reference_path = ARGV[arg + 2]
            prefix = "end-state: " events " "
            replays++
            stopped = 0; unknown_count = 0
            split("", state); split("", reference)
            if (mode == "baseline" && (getline line < reference_path) >= 0) {
                close(reference_path)
                print prefix "reference kept " reference_path " (delete it to re-baseline)"
                continue
            }
            n = read_sorted(state_path, state, 0)
            if (n < 0) { print prefix "no state"; failed = 1; continue }
            if (report_unclean(prefix, 0)) { failed = 1; continue }
            if (mode == "baseline") {
                printf "" > reference_path
                for (i = 1; i <= n; i++) print state[i] > reference_path
                close(reference_path)
                print prefix "reference written " reference_path
                continue
            }
            reference_n = read_sorted(reference_path, reference, 1)
            if (reference_n < 0) { print prefix "no reference"; failed = 1; continue }
            text = differences(reference, reference_n, state, n)
            if (text != "") { printf "%sdiffers\n%s", prefix, text; failed = 1 }
        }
        if (mode == "check" && !failed) print "end-state: " replays " replays match their reference"
        exit failed
    }
' "$@"
