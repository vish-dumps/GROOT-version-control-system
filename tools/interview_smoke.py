"""Run isolated CLI workflows; never modifies the source checkout's .groot."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("executable", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
executable = args.executable.resolve()
checks = []
calls = 0

with tempfile.TemporaryDirectory(prefix="groot-cli-interview-") as directory:
    root = Path(directory)

    def run(*arguments, ok=True):
        global calls
        result = subprocess.run([str(executable), *arguments], cwd=root,
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
        calls += 1
        assert (result.returncode == 0) == ok, (arguments, result.stdout, result.stderr)
        return result.stdout + result.stderr

    def put(path, content):
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_bytes(content)

    run("status", ok=False)
    run("init")
    run("init", ok=False)
    run("unknown", ok=False)
    checks.append("CLI validation and repository initialization")
    rng = random.Random(20261111)
    lengths = [0, 1, 2, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 129, 1024, 65536]
    samples = {f"data/{n}.bin": rng.randbytes(n) for n in lengths}
    for name, content in samples.items():
        put(name, content)
    put("space name.txt", b"same content")
    put("duplicate.txt", b"same content")
    put("shared.txt", b"one\ntwo\nthree\n")
    run("add", ".")
    for content in [*samples.values(), b"same content"]:
        digest = hashlib.sha256(content).hexdigest()
        assert (root / ".groot/objects" / digest).read_bytes() == content
    assert len(list((root / ".groot/objects").iterdir())) == len(lengths) + 2
    checks.append("16 SHA-256 boundary/binary vectors match Python hashlib; byte deduplication")
    assert "new empty file" in run("diff", "--staged")
    run("commit", "-m", "base")
    assert "working tree clean" in run("status")
    run("commit", "-m", "unchanged", ok=False)
    run("branch", "feature")
    run("checkout", "feature")
    put("shared.txt", b"ONE\ntwo\nthree\n")
    run("add", "shared.txt")
    run("commit", "-m", "first line")
    run("checkout", "main")
    put("shared.txt", b"one\ntwo\nTHREE\n")
    run("checkout", "feature", ok=False)
    assert "+THREE" in run("diff")
    run("add", "shared.txt")
    run("commit", "-m", "last line")
    assert "Merge commit" in run("merge", "feature")
    assert (root / "shared.txt").read_bytes() == b"ONE\ntwo\nTHREE\n"
    checks.append("Divergent same-file disjoint merge; dirty checkout protection")
    run("branch", "conflict")
    run("checkout", "conflict")
    put("shared.txt", b"LEFT\ntwo\nTHREE\n")
    run("add", "shared.txt")
    run("commit", "-m", "left")
    run("checkout", "main")
    put("shared.txt", b"RIGHT\ntwo\nTHREE\n")
    run("add", "shared.txt")
    run("commit", "-m", "right")
    assert "CONFLICT" in run("merge", "conflict")
    run("commit", "-m", "unresolved", ok=False)
    run("merge", "--abort")
    assert (root / "shared.txt").read_bytes() == b"RIGHT\ntwo\nTHREE\n"
    run("merge", "conflict")
    put("shared.txt", b"RESOLVED\ntwo\nTHREE\n")
    run("add", "shared.txt")
    run("commit", "-m", "resolved")
    assert not (root / ".groot/MERGE_HEAD").exists()
    assert "Already up to date" in run("merge", "conflict")
    checks.append("Conflict refusal, abort, staged resolution, and ancestor merge")
    (root / "data/0.bin").unlink()
    assert "deleted empty file" in run("diff")
    run("add", "data/0.bin")
    assert "deleted empty file" in run("diff", "--staged")
    run("commit", "-m", "delete empty")
    assert "working tree clean" in run("status")
    checks.append("Empty-file staged/unstaged deletion")
    digest = hashlib.sha256(samples["data/64.bin"]).hexdigest()
    (root / ".groot/objects" / digest).write_bytes(b"corrupt")
    run("commit", "-m", "unchanged", ok=False)
    put("extra.txt", b"extra")
    run("add", "extra.txt")
    assert "does not match" in run("commit", "-m", "corrupt", ok=False)
    checks.append("Corrupt blob prevents publishing a commit")

report = {"result": "PASS", "cli_invocations": calls, "checks": checks,
          "executable": str(executable), "temporary_repositories_removed": True}
if args.report:
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
