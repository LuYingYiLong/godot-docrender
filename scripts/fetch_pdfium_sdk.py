from argparse import ArgumentParser
from hashlib import file_digest
from pathlib import Path
from shutil import copyfileobj
from tarfile import open as open_archive
from tempfile import TemporaryDirectory
from urllib.parse import quote
from urllib.request import urlopen


RELEASE = "chromium/8152"
ARCHIVE = "pdfium-win-x64.tgz"
SHA256 = "cee2b0e9990bc67c0721774932961b9dbf2766046c10eec925acd4cfba6fdfe3"
SDK_FILES = ("include/fpdfview.h", "bin/pdfium.dll", "LICENSE", "licenses/pdfium.txt")


def check_sdk(directory: Path) -> bool:
    return all((directory / name).is_file() for name in SDK_FILES)


def main() -> None:
    parser = ArgumentParser(description="Install the pinned PDFium SDK for Windows x64.")
    parser.add_argument("--archive", type=Path, help=f"Install from a local copy of {ARCHIVE}.")
    args = parser.parse_args()
    sdk_dir = Path(__file__).resolve().parents[1] / "src" / "thirdparty" / "pdfium-sdk"
    checksum_file = sdk_dir / ".archive-sha256"

    if sdk_dir.exists():
        if checksum_file.is_file() and checksum_file.read_text(encoding="ascii").strip() == SHA256 and check_sdk(sdk_dir):
            print(f"PDFium {RELEASE} is already installed in {sdk_dir}")
            return
        raise SystemExit(f"A different or incomplete PDFium SDK is present in {sdk_dir}")

    with TemporaryDirectory(dir=sdk_dir.parent) as temp_dir:
        archive = args.archive
        if archive is None:
            archive = Path(temp_dir) / ARCHIVE
            tag = quote(RELEASE, safe="")
            url = f"https://github.com/bblanchon/pdfium-binaries/releases/download/{tag}/{ARCHIVE}"
            print(f"Downloading PDFium {RELEASE} for Windows x64...", flush=True)
            with urlopen(url, timeout=60) as response, archive.open("wb") as output:
                copyfileobj(response, output)

        with archive.open("rb") as source:
            if file_digest(source, "sha256").hexdigest() != SHA256:
                raise SystemExit("PDFium SDK checksum mismatch")
            source.seek(0)
            staging = Path(temp_dir) / "sdk"
            with open_archive(fileobj=source, mode="r:gz") as package:
                package.extractall(staging, filter="data")

        if not check_sdk(staging):
            raise SystemExit("PDFium SDK archive is missing required files")
        (staging / checksum_file.name).write_text(SHA256 + "\n", encoding="ascii", newline="\n")
        staging.rename(sdk_dir)

    print(f"Installed PDFium {RELEASE} in {sdk_dir}")


if __name__ == "__main__":
    main()
