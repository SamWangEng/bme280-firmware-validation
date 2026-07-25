from dataclasses import dataclass
from typing import Optional


@dataclass
class StatusLine:
    status: int              # e.g. 0x03
    temp: Optional[float]    # None when the firmware printed "--"
    err: str                 # e.g. "NONE" or "SENSOR_NOT_FOUND"


def parse_status_line(line: str) -> StatusLine:
    """Parses a line like 'STATUS:0x03 TEMP:23.41 ERR:NONE' into a StatusLine."""
    parts = line.split()
    if len(parts) != 3:
        raise ValueError(f"expected 3 fields, got {len(parts)}: {line!r}")

    fields = {}
    for part in parts:
        key, _, value = part.partition(":")
        fields[key] = value

    if not {"STATUS", "TEMP", "ERR"} <= fields.keys():
        raise ValueError(f"missing expected fields: {line!r}")

    status = int(fields["STATUS"], 16)
    temp = None if fields["TEMP"] == "--" else float(fields["TEMP"])
    err = fields["ERR"]

    return StatusLine(status=status, temp=temp, err=err)


if __name__ == "__main__":
    # quick self-test with known-good and known-bad example lines
    good = parse_status_line("STATUS:0x03 TEMP:23.41 ERR:NONE")
    assert good == StatusLine(status=0x03, temp=23.41, err="NONE"), good
    print("OK:", good)

    fault = parse_status_line("STATUS:0x04 TEMP:-- ERR:SENSOR_NOT_FOUND")
    assert fault == StatusLine(status=0x04, temp=None, err="SENSOR_NOT_FOUND"), fault
    print("OK:", fault)

    print("All self-tests passed.")
