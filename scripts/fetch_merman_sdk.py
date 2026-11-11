from hashlib import sha256
from json import loads
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.request import urlopen
from zipfile import ZipFile


VERSION = "0.9.3"
ARCHIVE = "merman-sdk-native-full-x86_64-pc-windows-msvc.zip"
SHA256 = "75c7f4d4b10a750152d93aead9d8c1d6bc44ef352d8d746f3bcbb1eaa622e5cf"


def main() -> None:
    sdk_dir = Path(__file__).resolve().parents[1] / "src" / "thirdparty" / "merman-sdk"
    if sdk_dir.exists():
        manifest = sdk_dir / "manifest.json"
        if (manifest.is_file() and loads(manifest.read_text(encoding="utf-8")).get("version") == VERSION
                and (sdk_dir / "include" / "merman.h").is_file()
                and (sdk_dir / "lib" / "merman_ffi.lib").is_file()):
            print(f"Merman {VERSION} is already installed in {sdk_dir}")
            return
        raise SystemExit(f"A different or incomplete Merman SDK is present in {sdk_dir}")

    url = f"https://github.com/Latias94/merman/releases/download/v{VERSION}/{ARCHIVE}"
    print(f"Downloading Merman {VERSION} for Windows x64 MSVC...", flush=True)
    with TemporaryDirectory(dir=sdk_dir.parent) as temp_dir:
        archive = Path(temp_dir) / ARCHIVE
        digest = sha256()
        with urlopen(url, timeout=60) as response, archive.open("wb") as output:
            for chunk in iter(lambda: response.read(1024 * 1024), b""):
                output.write(chunk)
                digest.update(chunk)
        if digest.hexdigest() != SHA256:
            raise SystemExit("Merman SDK checksum mismatch")
        staging = Path(temp_dir) / "sdk"
        with ZipFile(archive) as package:
            package.extractall(staging)
        staging.rename(sdk_dir)
    print(f"Installed Merman SDK in {sdk_dir}")


if __name__ == "__main__":
    main()
