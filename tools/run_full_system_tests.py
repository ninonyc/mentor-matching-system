#!/usr/bin/env python3
"""Phase 8: run isolated system suites and preserve an auditable results report."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-build', action='store_true', help='Use an already rebuilt C++ API')
    parser.add_argument('--browser', action='store_true', help='Also run the optional Playwright workflow')
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    directory = ROOT / 'build/phase8-results' / stamp
    directory.mkdir(parents=True, exist_ok=False)
    report = {'started_at_utc': stamp, 'environment': {
        'system': platform.system(), 'release': platform.release(),
        'macos': platform.mac_ver()[0], 'python': platform.python_version()},
        'checks': [], 'source_sha256': {}, 'browser_status': 'pending'}
    # Fingerprint relevant source files, never credentials or runtime databases.
    for folder in ('algorithm', 'backend/src', 'backend/tests', 'database', 'frontend', 'tests', 'tools'):
        for path in sorted((ROOT / folder).rglob('*')):
            if path.is_file() and path.suffix in ('.cpp', '.hpp', '.py', '.js', '.cjs', '.css', '.html', '.sql', '.sh') and '__pycache__' not in path.parts:
                report['source_sha256'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    env = dict(os.environ)
    env['PYTHON'] = sys.executable
    # Default to this Mac's actual OS only when the caller supplied no target.
    # Explicit MACOSX_DEPLOYMENT_TARGET remains authoritative.
    if platform.system() == 'Darwin' and 'MACOSX_DEPLOYMENT_TARGET' not in env:
        env['MACOSX_DEPLOYMENT_TARGET'] = platform.mac_ver()[0]
    report['environment']['macos_deployment_target'] = env.get('MACOSX_DEPLOYMENT_TARGET')

    def run(identifier, command, timeout=180):
        log = directory / (identifier + '.txt')
        print(identifier + ': ' + shlex.join(command), flush=True)
        started = time.monotonic()
        result = {'id': identifier, 'command': command, 'log': str(log.relative_to(ROOT))}
        try:
            with log.open('w') as stream:
                process = subprocess.run(command, cwd=ROOT, env=env, stdout=stream,
                                         stderr=subprocess.STDOUT, timeout=timeout)
            result['exit_code'] = process.returncode
            result['status'] = 'PASS' if process.returncode == 0 else 'FAIL'
        except subprocess.TimeoutExpired:
            result.update(status='FAIL', error='Timed out after ' + str(timeout) + ' seconds')
        except OSError as error:
            result.update(status='FAIL', error=str(error))
        result['seconds'] = round(time.monotonic() - started, 3)
        report['checks'].append(result)
        print('  ' + result['status'] + ' (' + str(result['seconds']) + 's)', flush=True)
        if result['status'] == 'FAIL':
            print('  Details: ' + str(log), flush=True)
        return result['status'] == 'PASS'

    build_ok = (ROOT / 'build/mentor_api').is_file() if args.skip_build else run('P8-build-api', ['bash', 'backend/build.sh'], 600)
    if args.skip_build:
        report['checks'].append({'id': 'P8-build-api', 'status': 'NOT_RUN',
            'reason': 'Caller requested --skip-build; existing API required'})
        if not build_ok:
            report['checks'].append({'id': 'P8-existing-api', 'status': 'FAIL', 'error': 'build/mentor_api is absent'})
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    targets = [
        ('matching_smoke_tests', ['algorithm/src/matching.cpp', 'tests/matching_smoke_tests.cpp']),
        ('stability_tests', ['algorithm/src/stability.cpp', 'tests/stability_tests.cpp']),
        ('core_validation', ['algorithm/src/matching.cpp', 'algorithm/src/stability.cpp', 'tests/core_validation.cpp']),
    ]
    for target, sources in targets:
        binary = directory / target
        if run('P8-build-' + target, compiler + ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                                               '-Ialgorithm/include'] + sources + ['-o', str(binary)]):
            run('P8-' + target, [str(binary)])
    run('P8-schema', [sys.executable, 'database/tests/test_schema.py'])
    if build_ok:
        run('P8-api', [sys.executable, '-W', 'default::ResourceWarning', 'backend/tests/test_api.py'])
        run('P8-persistence', [sys.executable, '-W', 'default::ResourceWarning', 'tests/integration/test_live_workflow.py'])
    else:
        report['checks'].append({'id': 'P8-http-suites', 'status': 'FAIL', 'error': 'No successful API build available'})
    node = shutil.which('node')
    if node:
        run('P8-frontend-controller', [node, '--test', 'tests/integration/frontend_restore.cjs'])
    else:
        report['checks'].append({'id': 'P8-frontend-controller', 'status': 'SKIP', 'reason': 'Node is not installed'})
        print('P8-frontend-controller: SKIP (Node is not installed)', flush=True)
    if args.browser:
        if node and build_ok:
            ok = run('P8-browser', [node, 'tests/integration/browser_workflow.cjs'])
            report['browser_status'] = 'passed' if ok else 'failed_or_blocked'
        else:
            report['checks'].append({'id': 'P8-browser', 'status': 'FAIL', 'error': 'Node and a successful API build are required'})
            report['browser_status'] = 'not_run_missing_requirement'
    failed = any(check['status'] == 'FAIL' for check in report['checks'])
    skipped = any(check['status'] == 'SKIP' for check in report['checks'])
    report['status'] = 'FAIL' if failed else 'PARTIAL' if skipped else 'EXECUTED_CHECKS_PASSED'
    report['finished_at_utc'] = datetime.now(timezone.utc).isoformat()
    destination = directory / 'results.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print('\n' + report['status'], flush=True)
    print('Browser acceptance: ' + report['browser_status'], flush=True)
    print('Report: ' + str(destination), flush=True)
    print('Phase 8 acceptance also requires the manual checklist; no deployment was performed.', flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
