"""Stable conformance verdict ABI shared with src/tests/conformance.h."""
import hashlib
import json


def verdict(suite, cases, error=None):
    rows = [{"id": key, "verdict": "PASS" if passed else "FAIL"}
            for key, passed in sorted(cases)]
    code = 2 if error or not rows else int(any(row["verdict"] == "FAIL" for row in rows))
    seeds = "\n".join(f"{suite}|{row['id']}|{row['verdict']}" for row in rows)
    result = {"@kind": "khanary.contract.verdict.v1", "suite": suite, "cases": rows,
              "verdict": ["PASS", "FAIL", "ERROR"][code], "exit_code": code,
              "summary_hash": "sha256:" + hashlib.sha256(seeds.encode()).hexdigest()}
    if error:
        result["error"] = error
    print(json.dumps(result, sort_keys=True, ensure_ascii=True, separators=(",", ":")))
    return code
