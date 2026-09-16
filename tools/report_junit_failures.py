#!/usr/bin/env python3
"""Expose pytest JUnit failures as GitHub Actions check annotations."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main() -> int:
    for argument in sys.argv[1:]:
        path = Path(argument)
        if not path.is_file():
            continue
        root = ET.parse(path).getroot()
        for case in root.iter("testcase"):
            problem = case.find("failure")
            if problem is None:
                problem = case.find("error")
            if problem is None:
                continue
            name = "{}.{}".format(case.get("classname", "test"),
                                  case.get("name", "failure"))
            message = problem.text or problem.get("message") or "test failed"
            print("::error title={}::{}".format(escape(name), escape(message)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
