# python
import argparse, subprocess, sys, difflib

def run_command(cmd: str) -> tuple[int, str]:
    try:
        out = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
        return 0, out.decode(errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    except subprocess.CalledProcessError as e:
        text = (e.output or b"").decode(errors="replace").replace("\r\n", "\n").replace("\r", "\n")
        return e.returncode, text

def main():
    p = argparse.ArgumentParser(description="Compare outputs of two shell commands.")
    p.add_argument("cmd1")
    p.add_argument("cmd2")
    p.add_argument("--strip", action="store_true", help="Strip leading/trailing whitespace before comparing.")
    p.add_argument("--context", type=int, default=3, help="Context lines for unified diff.")
    args = p.parse_args()

    c1, out1 = run_command(args.cmd1)
    c2, out2 = run_command(args.cmd2)

    a = out1.strip() if args.strip else out1
    b = out2.strip() if args.strip else out2

    equal = (c1 == 0 and c2 == 0 and a == b)
    print("Command 1 exit code:", c1)
    print("Command 2 exit code:", c2)
    print("Outputs equal:", "YES" if equal else "NO")

    if not equal:
        a_lines = a.splitlines(keepends=False)
        b_lines = b.splitlines(keepends=False)
        diff = difflib.unified_diff(a_lines, b_lines, fromfile="cmd1", tofile="cmd2", n=args.context)
        print("\n".join(diff) or "(no textual diff)")

    return 0 if equal else 1

if __name__ == "__main__":
    sys.exit(main())