#!/usr/bin/env python3
"""Offline E7 CODA evidence extraction. Never connects to or controls a phone.

Select one explicit PID in one log; cumulative snapshots are not independent runs.
Only whitelisted trace/state fields are emitted, never raw messages or endpoints.
Example: python summarize_coda_run.py --input PRIVATE_LOG --pid 1055 --output LOCAL_JSON
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

TRACE = re.compile(r"E7QW:TRACE ([a-zA-Z0-9_-]{1,64}) (\d+) (\d+) (\d+) (\d+) ([A-Z][A-Z0-9_]{0,63})(?=\s|$)")
R1_TRACE = re.compile(r"E7QT:R1 (\d+) (\d+) (\d+) (\d+) ([A-Z][A-Z0-9_]{0,63})(?=\s|$)")
STATE = re.compile(
    r"(E7QW|E7QT):STATE QMediaPlayer::(\w+) QMediaPlayer::(\w+) "
    r"(\d+) (-?\d+) (true|false) (true|false)"
)


def summarize(path, pid, first_line=1, last_line=None):
    data = Path(path).read_bytes()
    pid = str(pid).removeprefix("p")
    if not pid.isdigit():
        raise ValueError("PID must be numeric (optional p prefix)")
    if first_line < 1 or (last_line is not None and last_line < first_line):
        raise ValueError("Invalid inclusive source line range")
    traces, states, variants, other_pids = [], [], set(), set()
    owner = None
    last_ms = None
    identity = previous_sequence = previous_session = None
    clock_resets = malformed = 0
    for line, raw in enumerate(data.decode("utf-8-sig", errors="replace").splitlines(), 1):
        if line < first_line:
            continue
        if last_line is not None and line > last_line:
            break
        try:
            item = json.loads(raw)
        except ValueError:
            malformed += 1
            owner = None
            continue
        if not isinstance(item, dict):
            malformed += 1
            owner = None
            continue
        ms = item.get("ms")
        if isinstance(ms, (int, float)):
            if last_ms is not None and ms < last_ms:
                clock_resets += 1
                owner = None
            last_ms = ms
        # A new process/connection invalidates attribution of untagged state lines.
        if item.get("label") in ("Processes.start", "hello", "job_begin"):
            owner = None
        value = item.get("value")
        if not isinstance(value, list) or value[:3] != ["E", "Logging", "writeln"]:
            continue
        try:
            message = json.loads(value[-1])
        except (ValueError, TypeError):
            owner = None
            continue
        if not isinstance(message, str):
            continue
        t, r = TRACE.search(message), R1_TRACE.search(message)
        if t or r:
            variant, observed_pid, session, sequence, elapsed, event = (
                t.groups() if t else ("R1", *r.groups())
            )
            family = "E7QW" if t else "E7QT"
            owner = (observed_pid, family, int(session))
            if observed_pid == pid:
                observed_identity = (variant, family)
                if identity is not None and (identity != observed_identity or int(sequence) <= previous_sequence or int(session) < previous_session):
                    raise ValueError("Ambiguous PID/session or repeated trace sequence; select one job with --first-line/--last-line")
                identity, previous_sequence, previous_session = observed_identity, int(sequence), int(session)
                variants.add(variant)
                traces.append(dict(line=line, session=int(session), sequence=int(sequence),
                                   elapsed_ms=int(elapsed), event=event))
            else:
                other_pids.add(observed_pid)
        elif "E7QW:TRACE" in message or "E7QT:R1" in message:
            owner = None
        s = STATE.search(message)
        if s and owner and owner[:2] == (pid, s.group(1)):
            family, state, status, position, duration, video, audio = s.groups()
            states.append(dict(line=line, session=owner[2], state=state, status=status,
                               position_ms=int(position), duration_ms=int(duration),
                               video_available=video == "true", audio_available=audio == "true"))
    if not traces:
        raise ValueError("No matching E7QW/E7QT PID trace; legacy QTMW logs need manual attribution")
    return dict(
        schema=2, source_sha256=hashlib.sha256(data).hexdigest(), pid=pid,
        source_range=dict(first_line=first_line, last_line=last_line),
        variants=sorted(variants), other_pids=sorted(other_pids, key=int),
        host_clock_resets=clock_resets, malformed_records=malformed,
        first_trace_line=traces[0]["line"], last_trace_line=traces[-1]["line"],
        max_position_ms=max((s["position_ms"] for s in states), default=None),
        session_max_position_ms={str(session): max((s["position_ms"] for s in states if s["session"] == session), default=None)
                                 for session in sorted({t["session"] for t in traces})},
        first_frame_evidence="UNKNOWN", visible_motion="UNKNOWN", audible_sound="UNKNOWN",
        warnings=["Host ms can restart at reconnect; app elapsed_ms uses historical QTime.",
                  "PID is not a boot identifier; use job/variant/source range to disambiguate reuse.",
                  "State lines inherit the nearest same-family PID trace; they are not independently tagged.",
                  "Media sessions are listed separately; process-wide maximum does not imply one continuous playback.",
                  "No pass/fail or independent observation count is inferred from log filenames."],
        traces=traces, states=states,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--pid", required=True)
    parser.add_argument("--first-line", type=int, default=1, help="Inclusive line range for one launch job")
    parser.add_argument("--last-line", type=int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("Output must not overwrite the input evidence")
    result = summarize(args.input, args.pid, args.first_line, args.last_line)
    # Refuse to overwrite an existing result; preserve evidence provenance.
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("Extracted PID", result["pid"], "max position", result["max_position_ms"],
          "ms; visibility and first frame remain UNKNOWN")


if __name__ == "__main__":
    main()
