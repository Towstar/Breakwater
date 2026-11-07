"""Fetch the pinned MSVC Qt SDK into .tools; run with requests and py7zr installed.

Qt 6.12 uses a compiler-specific repository that older aqt metadata queries miss.
Archives come from Qt's repository and are checked against its published SHA-1.
"""
from pathlib import Path
import hashlib
import xml.etree.ElementTree as ET

import py7zr
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / ".tools" / "Qt" / "6.12.0" / "msvc2022_64"
CACHE = ROOT / ".tools" / "qt-downloads"
BASE = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/"
SDK_REVISION = "6.12.0-0-202609280346"
PACKAGES = {
    "qt.qt6.6120.win64_msvc2022_64": ("qtbase-", "qtdeclarative-", "qtsvg-", "qttools-"),
    "qt.qt6.6120.addons.qtshadertools.win64_msvc2022_64": ("qtshadertools-",),
    "qt.qt6.6120.addons.qtquick3d.win64_msvc2022_64": ("qtquick3d-",),
}


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    DESTINATION.mkdir(parents=True, exist_ok=True)
    session = requests.Session()
    session.mount("https://", HTTPAdapter(max_retries=Retry(total=3, backoff_factor=1, status_forcelist=(429, 500, 502, 503, 504))))
    response = session.get(BASE + "Updates.xml", timeout=30)
    response.raise_for_status()
    packages = ET.fromstring(response.content).findall("PackageUpdate")
    for package in packages:
        name = package.findtext("Name")
        if name not in PACKAGES:
            continue
        version = package.findtext("Version")
        if version != SDK_REVISION:
            raise RuntimeError("Qt SDK metadata revision changed; review and update SDK_REVISION before upgrading")
        for archive in package.findtext("DownloadableArchives", "").split(","):
            archive = archive.strip()
            if not archive.startswith(PACKAGES[name]):
                continue
            url = BASE + name + "/" + version + archive
            checksum = session.get(url + ".sha1", timeout=30)
            checksum.raise_for_status()
            expected = checksum.text.strip().split()[0].lower()
            local = CACHE / archive
            if not local.exists() or hashlib.sha1(local.read_bytes()).hexdigest() != expected:
                print("Downloading", archive, flush=True)
                with session.get(url, stream=True, timeout=120) as download:
                    download.raise_for_status()
                    with local.open("wb") as output:
                        for chunk in download.iter_content(1024 * 1024):
                            output.write(chunk)
            if hashlib.sha1(local.read_bytes()).hexdigest() != expected:
                raise RuntimeError("Qt archive checksum mismatch: " + archive)
            marker = CACHE / (archive + ".extracted")
            marker_value = expected + "\n" + str(DESTINATION)
            if marker.exists() and marker.read_text(encoding="utf-8") == marker_value:
                continue
            print("Extracting", archive, flush=True)
            with py7zr.SevenZipFile(local, mode="r") as sdk:
                for member in sdk.getnames():
                    target = (DESTINATION / member).resolve()
                    if not target.is_relative_to(DESTINATION.resolve()):
                        raise RuntimeError("Unsafe archive path")
                sdk.extractall(path=DESTINATION)
            marker.write_text(marker_value, encoding="utf-8")
    prefix = DESTINATION
    if not (prefix / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
        raise RuntimeError("Expected Qt SDK layout missing after extraction")
    (prefix / "bin" / "qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
    print("Qt SDK:", prefix, flush=True)


if __name__ == "__main__":
    main()
