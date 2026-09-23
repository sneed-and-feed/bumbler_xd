import os
import zipfile
import hashlib

root_dir = r"c:\Users\x\Documents\antigravity\bumbler_xd"
release_dir = os.path.join(root_dir, "releases")
build_dir = os.path.join(root_dir, "build", "BumblerXD_artefacts", "Release")

os.makedirs(release_dir, exist_ok=True)
version = "1.0.0"

# 1. Package Windows-x64 full zip (Standalone + VST3 + docs)
zip_path = os.path.join(release_dir, f"BUMBLER_XD-v{version}-Windows-x64.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "Bumbler XD.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    standalone_file = os.path.join(build_dir, "Standalone", "Bumbler XD.exe")
    if os.path.exists(standalone_file):
        zf.write(standalone_file, "Bumbler XD.exe")
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {zip_path}: {os.path.getsize(zip_path):,} bytes")

# 2. Package VST3-only zip
vst3_zip_path = os.path.join(release_dir, f"BUMBLER_XD-v{version}-VST3-Windows-x64.zip")
with zipfile.ZipFile(vst3_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "Bumbler XD.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {vst3_zip_path}: {os.path.getsize(vst3_zip_path):,} bytes")

# 3. Compute SHA-256 digests and write SHA256SUMS.txt
def compute_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

sha256_path = os.path.join(release_dir, "SHA256SUMS.txt")
zip_files = sorted([f for f in os.listdir(release_dir) if f.endswith(".zip") and f.startswith("BUMBLER_XD")])

checksum_lines = []
for zfname in zip_files:
    zpath = os.path.join(release_dir, zfname)
    digest = compute_sha256(zpath)
    checksum_lines.append(f"{digest}  {zfname}\n")
    print(f"SHA-256 ({zfname}) = {digest}")

with open(sha256_path, "w", encoding="utf-8") as f:
    f.writelines(checksum_lines)

print(f"Created {sha256_path} with {len(checksum_lines)} checksums.")
