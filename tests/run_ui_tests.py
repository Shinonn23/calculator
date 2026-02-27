#!/usr/bin/env python3
"""UI test runner for math-solver .msl scripts.

Each test is a .msl file paired with a .stderr file containing expected output.
Run with --bless to regenerate expected files when output changes intentionally.

Usage:
    run_ui_tests.py [--binary PATH] [--tests-dir DIR] [--bless]
"""

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path


ANSI_ESCAPE = re.compile(r'\x1b\[[0-9;]*[mGKH]')


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE.sub('', text)


def normalize_output(text: str, msl_path: Path) -> str:
    text = strip_ansi(text)
    # Replace absolute path with just the filename so .stderr files are portable
    text = text.replace(str(msl_path), msl_path.name)
    # Replace $HOME with ~ so :config path output is portable across machines
    text = text.replace(str(Path.home()), '~')
    return text


def run_test(binary: Path, msl_file: Path, bless: bool) -> bool:
    """Run a single test. Returns True on pass."""
    stderr_file = msl_file.with_suffix('.stderr')

    result = subprocess.run(
        [str(binary), '--script', str(msl_file)],
        capture_output=True,
        text=True,
    )

    # Combine stdout and stderr; the binary writes results to stdout and
    # diagnostics to stderr.
    raw = result.stdout + result.stderr
    normalized = normalize_output(raw, msl_file)

    if bless:
        stderr_file.write_text(normalized, encoding='utf-8')
        print(f'BLESSED  {msl_file.name}')
        return True

    if not stderr_file.exists():
        print(f'MISSING  {msl_file.name}  (no .stderr file; run with --bless)')
        return False

    expected = stderr_file.read_text(encoding='utf-8')
    if normalized == expected:
        print(f'ok       {msl_file.name}')
        return True

    print(f'FAIL     {msl_file.name}')
    diff = difflib.unified_diff(
        expected.splitlines(keepends=True),
        normalized.splitlines(keepends=True),
        fromfile=f'{msl_file.name}.stderr (expected)',
        tofile=f'{msl_file.name}.stderr (actual)',
    )
    sys.stdout.writelines(diff)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description='Run math-solver UI tests')
    parser.add_argument(
        '--binary',
        type=Path,
        default=Path('./build/bin/math-solver'),
        help='Path to math-solver binary',
    )
    parser.add_argument(
        '--tests-dir',
        type=Path,
        default=Path('./tests/ui'),
        help='Directory containing .msl test files',
    )
    parser.add_argument(
        '--bless',
        action='store_true',
        help='Regenerate .stderr expected output files',
    )
    args = parser.parse_args()

    if not args.binary.exists():
        print(f'error: binary not found: {args.binary}', file=sys.stderr)
        return 1

    msl_files = sorted(args.tests_dir.rglob('*.msl'))
    if not msl_files:
        print(f'warning: no .msl files found in {args.tests_dir}', file=sys.stderr)
        return 0

    passed = 0
    failed = 0
    for msl_file in msl_files:
        if run_test(args.binary, msl_file, args.bless):
            passed += 1
        else:
            failed += 1

    total = passed + failed
    if args.bless:
        print(f'\nBlessed {total} test(s).')
        return 0

    print(f'\n{passed}/{total} passed', end='')
    if failed:
        print(f', {failed} failed')
        return 1
    print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
