import datetime
import sqlite3

import pytest

DB_PATH = "test_results.db"

# Generated once when conftest.py is loaded - i.e. once per `pytest` invocation -
# so every row logged during this run shares the same run_id, letting the report
# script group and summarize "the most recent run" instead of the whole history.
RUN_ID = datetime.datetime.now().isoformat(timespec="seconds")


def _get_conn():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            test_name TEXT NOT NULL,
            outcome TEXT NOT NULL,
            raw_reading TEXT
        )
    """)
    return conn

# pytest_runtest_makereport is one of pytest's built-in hook names
# item means the test function in the test_firmware.py 
@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    # pause this particular call, let pytest finish generating the report for whichever single phase triggered this invocation, then resume with that result. If this invocation was triggered by the setup phase, yield waits for the setup report; if triggered by call, it waits for the call report; if by teardown, the teardown report.
    outcome = yield
    report = outcome.get_result()

    # runs 3 separate times per test, this check is what discards the setup and teardown invocations
    if report.when != "call":
        return  # ignore setup/teardown phases, only log the actual test run

    raw_reading = None
    for name, value in item.user_properties: # item.user_properties is a list of any extra key/value data the test attached to itself via record_property(...) 
        if name == "raw_reading":
            raw_reading = value

    conn = _get_conn()
    conn.execute(
        "INSERT INTO results (run_id, timestamp, test_name, outcome, raw_reading) VALUES (?, ?, ?, ?, ?)",
        (
            RUN_ID,
            datetime.datetime.now().isoformat(timespec="seconds"),
            item.name,
            report.outcome,
            raw_reading,
        ),
    )
    conn.commit()
    conn.close()
