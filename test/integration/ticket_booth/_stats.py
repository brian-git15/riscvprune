#!/usr/bin/env python3
"""Compare ticket_booth linked IR vs nounwind-lto output."""
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "test/integration/ticket_booth/_out"
LINKED = (OUT / "ticket_booth.linked.ll").read_text(encoding="utf-8")
PASSED = (OUT / "ticket_booth.nounwind.ll").read_text(encoding="utf-8")

DEFINE = re.compile(r"^define[^\n]+@([^\(\s]+)\([^\n]*", re.M)
ATTRS = re.compile(r"^attributes\s+(#\d+)\s*=\s*\{([^}]*)\}", re.M)
INVOKE_CALLEE = re.compile(r"\binvoke\s+[^\n]*?@([^\(\s]+)")


def parse_funcs(text: str):
    funcs = {}
    for m in DEFINE.finditer(text):
        line = m.group(0)
        name = m.group(1)
        # Attr group may be followed by `align N` / `comdat` / `{`.
        am = re.search(r"\s#(\d+)(?:\s|$)", line)
        funcs[name] = {
            "attr_ref": f"#{am.group(1)}" if am else None,
            "inline_nounwind": bool(re.search(r"(?<![a-z])nounwind(?![a-z])", line)),
        }
    return funcs


def parse_attr_groups(text: str):
    return {m.group(1): m.group(2) for m in ATTRS.finditer(text)}


def has_nounwind(func, groups) -> bool:
    if func["inline_nounwind"]:
        return True
    ref = func["attr_ref"]
    return bool(ref and ref in groups and "nounwind" in groups[ref])


def pretty(n: str) -> str:
    if n == "main":
        return "main"
    if "CheerfulAttendant" in n:
        if "tipOnWin" in n:
            return "CheerfulAttendant::tipOnWin"
        if "nameEv" in n:
            return "CheerfulAttendant::name"
        if "D0Ev" in n or "D1Ev" in n or "D2Ev" in n:
            return "CheerfulAttendant::~"
        return "CheerfulAttendant"
    if "GrumpyAttendant" in n:
        if "tipOnWin" in n:
            return "GrumpyAttendant::tipOnWin"
        if "nameEv" in n:
            return "GrumpyAttendant::name"
        if "D0Ev" in n or "D1Ev" in n or "D2Ev" in n:
            return "GrumpyAttendant::~"
        return "GrumpyAttendant"
    if "Attendant" in n and ("D0Ev" in n or "D1Ev" in n or "D2Ev" in n):
        return "Attendant::~"
    table = [
        ("TicketBoothC", "TicketBooth::TicketBooth"),
        ("TicketBooth3run", "TicketBooth::run"),
        ("TicketBooth11ticketsLeft", "TicketBooth::ticketsLeft"),
        ("ClawMachineC", "ClawMachine::ClawMachine"),
        ("ClawMachine11attemptGrab", "ClawMachine::attemptGrab"),
        ("ClawMachine5plays", "ClawMachine::plays"),
        ("LedgerC", "Ledger::Ledger"),
        ("Ledger8canAfford", "Ledger::canAfford"),
        ("Ledger7balance", "Ledger::balance"),
        ("Ledger5spend", "Ledger::spend"),
        ("Ledger4earn", "Ledger::earn"),
        ("RngC", "Rng::Rng"),
        ("Rng4nextEv", "Rng::next"),
        ("Rng11nextBounded", "Rng::nextBounded"),
        ("8tierName", "tierName"),
        ("8tierCost", "tierCost"),
        ("8pickTier", "pickTier"),
        ("__clang_call_terminate", "__clang_call_terminate"),
        ("__cxa_", n),
        ("_ZSt", "libstdc++"),
    ]
    for needle, label in table:
        if needle in n:
            return label if label != n else n
    return n


def count_calls(text: str) -> int:
    n = 0
    for line in text.splitlines():
        if "invoke" in line:
            continue
        if re.search(r"(=\s*call\s|\bcall\s)", line):
            n += 1
    return n


def analyze():
    lg, pg = parse_attr_groups(LINKED), parse_attr_groups(PASSED)
    lf, pf = parse_funcs(LINKED), parse_funcs(PASSED)

    inv_before = len(re.findall(r"\binvoke\b", LINKED))
    inv_after = len(re.findall(r"\binvoke\b", PASSED))
    calls_before = count_calls(LINKED)
    calls_after = count_calls(PASSED)

    nw_before = sum(1 for f in lf.values() if has_nounwind(f, lg))
    nw_after = sum(1 for f in pf.values() if has_nounwind(f, pg))

    gained, already, still_no = [], [], []
    for name in sorted(lf):
        b = has_nounwind(lf[name], lg)
        a = has_nounwind(pf[name], pg)
        row = {"name": name, "pretty": pretty(name), "before": b, "after": a}
        if a and not b:
            gained.append(row)
        elif a and b:
            already.append(row)
        else:
            still_no.append(row)

    li = Counter(INVOKE_CALLEE.findall(LINKED))
    pi = Counter(INVOKE_CALLEE.findall(PASSED))
    invoke_rows = []
    for k in sorted(set(li) | set(pi), key=lambda x: (-(li.get(x, 0) - pi.get(x, 0)), -li.get(x, 0), x)):
        b, a = li.get(k, 0), pi.get(k, 0)
        invoke_rows.append(
            {
                "callee": k,
                "pretty": pretty(k),
                "before": b,
                "after": a,
                "lowered": b - a,
            }
        )

    result = {
        "defined_funcs": len(lf),
        "invokes": {"before": inv_before, "after": inv_after, "lowered": inv_before - inv_after},
        "calls": {"before": calls_before, "after": calls_after, "delta": calls_after - calls_before},
        "nounwind_funcs": {"before": nw_before, "after": nw_after, "gained": len(gained)},
        "landingpad": {"before": LINKED.count("landingpad"), "after": PASSED.count("landingpad")},
        "resume": {
            "before": len(re.findall(r"\bresume\b", LINKED)),
            "after": len(re.findall(r"\bresume\b", PASSED)),
        },
        "gained": gained,
        "already": already,
        "still_no": still_no,
        "invoke_by_callee": invoke_rows,
        "invoke_lower_rate": (
            round(100.0 * (inv_before - inv_after) / inv_before, 1) if inv_before else 0.0
        ),
        "nounwind_coverage_after": (
            round(100.0 * nw_after / len(lf), 1) if lf else 0.0
        ),
    }
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    analyze()
