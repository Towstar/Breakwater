from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[3]
def git_file(ref,path):
    return subprocess.check_output(['git','show',f'{ref}:{path}'],cwd=root).decode('utf-8')
def resolve(path,choices):
    target=root/path
    text=target.read_bytes().decode('utf-8')
    pattern=re.compile(r'^<<<<<<< HEAD\r?\n(.*?)^=======\r?\n(.*?)^>>>>>>> origin/main\r?\n',re.M|re.S)
    matches=list(pattern.finditer(text)); assert len(matches)==len(choices),(path,len(matches))
    for match,choice in reversed(list(zip(matches,choices))):
        ours,theirs=match.group(1),match.group(2)
        replacement={'ours':ours,'theirs':theirs,'both':ours+theirs}[choice]
        text=text[:match.start()]+replacement+text[match.end():]
    target.write_bytes(text.encode('utf-8'))

# Main already implements the same bounded exact grid and supports selected
# derivatives. Retain its complete validator/API rather than the old forward-only API.
(root/'desktop/science/WorkerProtocol.cpp').write_bytes(git_file('origin/main','desktop/science/WorkerProtocol.cpp').encode())
resolve('desktop/tests/storage_science_tests.cpp',['ours'])
target=root/'desktop/tests/storage_science_tests.cpp'
text=target.read_text(encoding='utf-8')
needle='        for (const auto& item : storage_grid_test::cases())\n'
assert needle in text
text=text.replace(needle,'        QTest::newRow("ratio-underflow-large-step") << QString("mixed") << std::numeric_limits<double>::denorm_min() << 1e308;\n'+needle,1)
target.write_text(text,encoding='utf-8')

resolve('desktop/tests/storage_worker_tests.cpp',['both','ours','both'])
target=root/'desktop/tests/storage_worker_tests.cpp'
text=target.read_text(encoding='utf-8').replace('protocol::forwardPayload(', 'protocol::storagePayload(')
needle='        QTest::newRow("inherited-one-ULP-endpoint") << 0.225 << 0.075 << true << 256;\n'
assert needle in text
text=text.replace(needle,needle+'        QTest::newRow("ratio-underflow-large-step") << std::numeric_limits<double>::denorm_min() << 1e308 << false << 256;\n'+
    '        QTest::newRow("renamed-rounded-budget") << 0.0082 << 8.2e-7 << true << 256;\n')
target.write_text(text,encoding='utf-8')

resolve('desktop/tests/worker_fixtures/storage_worker_fixture.py',['theirs','both'])
target=root/'desktop/tests/worker_fixtures/storage_worker_fixture.py'
text=target.read_text(encoding='utf-8')
assert 'import math\n' not in text
target.write_text(text.replace('import json\n','import json\nimport math\n',1),encoding='utf-8')

# Retain all new-main status/history sections and preserve the earlier frozen
# combined-verification entry as a historical record below the new addendum.
status=git_file('origin/main','docs/STATUS.md')
ours=git_file('HEAD','docs/STATUS.md')
section=ours.split('## Combined verification',1)[1].split('## PR #19',1)[0]
historical='## Combined verification'+section
prefix='''## PR #26 reconciliation with current main — November 7, 2026

- Integrating `e057fa4` into the verified branch, retaining selected-control
  sensitivities, automatic scientific-history retention and immutable historical
  reopening. Exact-grid and lifecycle regressions are combined with the new suites.
- Affected normal/MSVC ASan, real-worker, history, rendering and installed checks
  are in progress; earlier frozen-baseline evidence below keeps its original scope.
- The local continuation plan remains untracked and excluded from the PR.

'''
assert status.startswith('# Implementation status\n\n')
status='# Implementation status\n\n'+prefix+historical+status[len('# Implementation status\n\n'):]
(root/'docs/STATUS.md').write_text(status,encoding='utf-8')
