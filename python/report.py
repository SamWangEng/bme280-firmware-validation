import sqlite3
import sys

DB_PATH = "test_results.db"


def main():
    conn = sqlite3.connect(DB_PATH)

    latest_run = conn.execute(
        "SELECT run_id FROM results ORDER BY timestamp DESC LIMIT 1"
    ).fetchone()

    if latest_run is None:
        print("No test results found - run pytest first.")
        return

    run_id = latest_run[0]
    rows = conn.execute(
        "SELECT test_name, outcome, raw_reading FROM results WHERE run_id = ? ORDER BY id",
        (run_id,),
    ).fetchall()
    conn.close()

    total = len(rows)
    passed = [r for r in rows if r[1] == "passed"]
    failed = [r for r in rows if r[1] == "failed"]

    print(f"Test run: {run_id}")
    print(f"Total: {total}   Passed: {len(passed)}   Failed: {len(failed)}")
    print()

    for test_name, outcome, raw_reading in rows:
        mark = "PASS" if outcome == "passed" else "FAIL"
        print(f"[{mark}] {test_name}")
        if raw_reading:
            print(f"       {raw_reading}")

    print()
    if failed:
        print(f"RESULT: FAIL ({len(failed)} of {total} tests failed)")
        sys.exit(1)
    else:
        print(f"RESULT: PASS (all {total} tests passed)")
        sys.exit(0)


if __name__ == "__main__":
    main()
