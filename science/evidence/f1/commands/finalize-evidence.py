from pathlib import Path
import hashlib,json,re,subprocess
from urllib.parse import unquote,urlsplit
import importlib.metadata as metadata

root=Path(__file__).resolve().parents[1];folder=root/'science/evidence/f1'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
broken=[]
documents=['science/FRONT_ACCURACY.md','science/OBSERVATION_SEMANTICS.md','science/README.md','science/PROGRESS.md','engine/TRANSPORT.md']
for name in documents:
    for link in re.findall(r'\]\(([^)]+)\)',(root/name).read_text(encoding='utf-8')):
        parts=urlsplit(link)
        if parts.scheme or link.startswith('#'):continue
        if not ((root/name).parent/unquote(parts.path)).exists():broken.append([name,link])
assert not broken,broken
manifest=json.loads((folder/'manifest.json').read_text())
assert all(digest(root/path)==value for path,value in manifest['sources'].items())
assert manifest['verified_code_commit']=='fffe5c342caf64725c6242215a8858280ca75441'
git=lambda *args:subprocess.check_output(['git',*args],cwd=root).decode().strip()
assert not git('diff','de06aa1','--name-only','--','engine/src','engine/include','engine/CMakeLists.txt','desktop','contracts','proofs','docs','CMakeLists.txt','CMakePresets.json')
assert not git('ls-files','*PLAN_LOCAL*','*LOCAL_PLAN*')
source_config=digest(root/'science/f1_front_accuracy_config.json')
first=json.loads((root/'out/f1/attempt-02-normal/report.json').read_text())
assert first['config_sha256']==source_config
runtime=dict(python=__import__('sys').version,executable=__import__('sys').executable,
    packages={name:metadata.version(name) for name in ['numpy','torch','nvidia-physicsnemo','jsonschema']})
(folder/'runtime.json').write_bytes((json.dumps(runtime,indent=2)+'\n').encode())
checks=dict(relative_links='passed',source_hashes='passed',compiled_solver_and_public_contracts_unchanged=True,
    predeclared_config_matches_first_native_measurement=True,local_continuation_plan_excluded=True,
    scope='Read-only source/metadata checks after documentation; runtime code stays at the recorded verified commit.')
(folder/'final-checks.json').write_bytes((json.dumps(checks,indent=2)+'\n').encode())
files=[p for p in folder.rglob('*') if p.is_file() and p.name!='SHA256SUMS.json']
files += [root/'science/FRONT_ACCURACY.md',root/'science/OBSERVATION_SEMANTICS.md',root/'science/f1_front_accuracy_config.json']
checksums={p.relative_to(root).as_posix():digest(p) for p in sorted(files)}
(folder/'SHA256SUMS.json').write_bytes((json.dumps(checksums,indent=2)+'\n').encode())
print(f'PASS: {len(checksums)} evidence/declaration/report hashes, relative links, source scope, unchanged predeclaration and local-plan exclusion')
