import ctypes
import hashlib
import os
import sys
import zipfile
from pathlib import Path

root_dir = str(Path(__file__).resolve().parent.parent)
release_dir = os.path.join(root_dir, "releases")
build_dir = os.path.join(root_dir, "build", "BumblerXD_artefacts", "Release")

os.makedirs(release_dir, exist_ok=True)
version = "1.1.0"


def verify_windows_pe_version(pe_path: str, expected_version: str) -> None:
    if not os.path.isfile(pe_path):
        raise FileNotFoundError(f"Missing expected binary: {pe_path}")
    if sys.platform != "win32":
        return

    ver_dll = ctypes.windll.version
    dummy = ctypes.c_ulong(0)
    size = ver_dll.GetFileVersionInfoSizeW(pe_path, ctypes.byref(dummy))
    if size <= 0:
        raise RuntimeError(f"Missing VERSIONINFO resource in {pe_path}")

    buf = ctypes.create_string_buffer(size)
    if not ver_dll.GetFileVersionInfoW(pe_path, 0, size, buf):
        raise RuntimeError(f"GetFileVersionInfoW failed for {pe_path}")

    def query_string(field: str) -> str:
        ptr = ctypes.c_void_p()
        length = ctypes.c_uint(0)
        sub_block = f"\\StringFileInfo\\040904E4\\{field}"
        if not ver_dll.VerQueryValueW(buf, sub_block, ctypes.byref(ptr), ctypes.byref(length)) or not ptr.value:
            raise RuntimeError(f"Missing VERSIONINFO field '{field}' in {pe_path}")
        return ctypes.wstring_at(ptr.value).rstrip("\x00").strip()

    file_ver = query_string("FileVersion")
    prod_ver = query_string("ProductVersion")
    if file_ver != expected_version or prod_ver != expected_version:
        raise RuntimeError(
            f"Stale PE VERSIONINFO in {pe_path}: FileVersion='{file_ver}', "
            f"ProductVersion='{prod_ver}' (expected '{expected_version}')"
        )
    print(f"Verified PE VERSIONINFO ({os.path.basename(pe_path)}): FileVersion={file_ver}, ProductVersion={prod_ver}")


vst3_dir = os.path.join(build_dir, "VST3", "Bumbler XD.vst3")
vst3_binary = os.path.join(vst3_dir, "Contents", "x86_64-win", "Bumbler XD.vst3")
standalone_file = os.path.join(build_dir, "Standalone", "Bumbler XD.exe")

verify_windows_pe_version(standalone_file, version)
verify_windows_pe_version(vst3_binary, version)

full_zip_name = f"BUMBLER_XD-v{version}-Windows-x64.zip"
vst3_zip_name = f"BUMBLER_XD-v{version}-VST3-Windows-x64.zip"
active_archives = {full_zip_name, vst3_zip_name}

# Remove stale release archives from older versions so releases/ stays clean
for existing in os.listdir(release_dir):
    if existing.startswith("BUMBLER_XD-v") and existing.endswith(".zip") and existing not in active_archives:
        stale_path = os.path.join(release_dir, existing)
        os.remove(stale_path)
        print(f"Removed stale archive: {stale_path}")

# 1. Package Windows-x64 full zip (Standalone + VST3 + docs + migrator)
zip_path = os.path.join(release_dir, full_zip_name)
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(vst3_dir):
        dirs.sort()
        for f in sorted(files):
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    if os.path.exists(standalone_file):
        zf.write(standalone_file, "Bumbler XD.exe")
    if os.path.exists(os.path.join(root_dir, "MigratePresets.bat")):
        zf.write(os.path.join(root_dir, "MigratePresets.bat"), "MigratePresets.bat")
    if os.path.exists(os.path.join(root_dir, "scripts", "migrator_gui.py")):
        zf.write(os.path.join(root_dir, "scripts", "migrator_gui.py"), "scripts/migrator_gui.py")
    if os.path.exists(os.path.join(root_dir, "scripts", "import_wasp_presets.py")):
        zf.write(os.path.join(root_dir, "scripts", "import_wasp_presets.py"), "scripts/import_wasp_presets.py")
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {zip_path}: {os.path.getsize(zip_path):,} bytes")

# 2. Package VST3-only zip
vst3_zip_path = os.path.join(release_dir, vst3_zip_name)
with zipfile.ZipFile(vst3_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(vst3_dir):
        dirs.sort()
        for f in sorted(files):
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {vst3_zip_path}: {os.path.getsize(vst3_zip_path):,} bytes")


# 3. Compute SHA-256 digests and write SHA256SUMS.txt (active v1.1.0 archives only)
def compute_sha256(filepath: str) -> str:
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


sha256_path = os.path.join(release_dir, "SHA256SUMS.txt")
zip_files = sorted(active_archives)

checksum_lines = []
for zfname in zip_files:
    zpath = os.path.join(release_dir, zfname)
    digest = compute_sha256(zpath)
    checksum_lines.append(f"{digest}  {zfname}\n")
    print(f"SHA-256 ({zfname}) = {digest}")

with open(sha256_path, "w", encoding="utf-8") as f:
    f.writelines(checksum_lines)

print(f"Created {sha256_path} with {len(checksum_lines)} checksums.")
