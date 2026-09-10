import json
from pathlib import Path
import tempfile
import unittest
from summarize_coda_run import summarize


def log(message, ms=1):
    return json.dumps({"ms": ms, "label": "event", "value":
                      ["E", "Logging", "writeln", '"ProgramOutputConsoleLogger"', json.dumps(message)]})


class AttributionTests(unittest.TestCase):
    def run_summary(self, lines, pid="8", **kwargs):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "input.jsonl"
            path.write_text("\n".join(lines), encoding="utf-8")
            return summarize(path, pid, **kwargs)

    def test_cumulative_snapshot_filters_other_process_and_never_infers_picture(self):
        result = self.run_summary([
            log("E7QW:TRACE old 7 1 1 10 SAMPLE"),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 99000 727249 true true"),
            log("E7QW:TRACE new 8 1 1 10 SAMPLE"),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 1200 727249 true true"),
        ])
        self.assertEqual(result["max_position_ms"], 1200)
        self.assertEqual(result["other_pids"], ["7"])
        self.assertEqual(result["first_frame_evidence"], "UNKNOWN")
        self.assertEqual(result["audible_sound"], "UNKNOWN")

    def test_reconnect_and_other_family_invalidate_untagged_state(self):
        result = self.run_summary([
            log("E7QT:R1 8 1 1 10 SAMPLE", 100),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 9000 727249 true true", 101),
            log("E7QT:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 8000 727249 true true", 1),
        ])
        self.assertIsNone(result["max_position_ms"])
        self.assertEqual(result["host_clock_resets"], 1)

    def test_missing_pid_fails_closed(self):
        with self.assertRaises(ValueError):
            self.run_summary([log("E7QW:TRACE wrong 7 1 1 10 SAMPLE")])

    def test_raw_payload_and_malformed_lines_are_not_exported(self):
        result = self.run_summary([
            log("E7QW:TRACE new 8 1 1 10 SAMPLE"),
            log("unrelated PRIVATE_TOKEN"), "{invalid", "[]",
        ])
        self.assertEqual(result["malformed_records"], 2)
        self.assertNotIn("PRIVATE_TOKEN", json.dumps(result))

    def test_pid_reuse_requires_explicit_job_range(self):
        lines = [
            log("E7QW:TRACE new 8 1 1 10 SAMPLE"),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 99000 727249 true true"),
            log("E7QW:TRACE new 8 1 1 10 SAMPLE"),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 1200 727249 true true"),
        ]
        with self.assertRaisesRegex(ValueError, "Ambiguous PID"):
            self.run_summary(lines)
        result = self.run_summary(lines, first_line=3, last_line=4)
        self.assertEqual(result["max_position_ms"], 1200)
        self.assertEqual(result["source_range"], dict(first_line=3, last_line=4))

    def test_malformed_trace_cannot_reassign_an_untagged_state(self):
        result = self.run_summary([
            log("E7QW:TRACE new 8 1 1 10 SAMPLE"),
            log("E7QW:TRACE bad 7 1 1 10 https://private.invalid/token"),
            log("E7QW:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 99000 727249 true true"),
        ])
        self.assertIsNone(result["max_position_ms"])
        self.assertNotIn("private.invalid", json.dumps(result))

    def test_r1_initialization_and_retry_sessions_are_not_pid_reuse(self):
        result = self.run_summary([
            log("E7QT:R1 8 0 1 1 R1_CREATED"),
            log("E7QT:R1 8 1 2 10 WAIT_MAIN_UI_AND_QGL"),
            log("E7QT:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 1200 727249 true true"),
            log("E7QT:R1 8 2 3 20 WAIT_MAIN_UI_AND_QGL"),
            log("E7QT:STATE QMediaPlayer::PlayingState QMediaPlayer::BufferedMedia 0 727249 true true"),
        ])
        self.assertEqual(result["session_max_position_ms"], {"0": None, "1": 1200, "2": 0})


if __name__ == "__main__":
    unittest.main()
