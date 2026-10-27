"""Install the laptop bridge and ESP32 build requirements on this computer."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import venv


ROOT = Path(__file__).resolve().parent
ENV_DIR = ROOT / ".venv"


def environment_paths(env_dir=ENV_DIR, windows=None):
    if windows is None:
        windows = os.name == "nt"
    folder = env_dir / ("Scripts" if windows else "bin")
    return folder / ("python.exe" if windows else "python"), folder / ("pio.exe" if windows else "pio")


def run(command, dry_run=False):
    print("$ " + subprocess.list2cmdline([str(part) for part in command]), flush=True)
    if not dry_run:
        subprocess.run(command, cwd=ROOT, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Show install commands without changing files")
    args = parser.parse_args(argv)
    if sys.version_info < (3, 9):
        parser.error("Python 3.9 or newer is required")

    python, pio = environment_paths()
    if not python.exists():
        print(f"Creating virtual environment at {ENV_DIR}", flush=True)
        if not args.dry_run:
            try:
                venv.create(ENV_DIR, with_pip=True)
            except Exception as error:
                parser.exit(1, f"Could not create virtual environment: {error}\n")
    run([python, "-m", "pip", "install", "-r", ROOT / "requirements-bridge.txt", "platformio"], args.dry_run)
    run([pio, "pkg", "install", "--project-dir", ROOT / "esp32"], args.dry_run)

    print("\nInstall plan shown. Run without --dry-run to install." if args.dry_run else "\nInstalled. To build firmware:")
    if args.dry_run:
        print("After installation, build firmware with:")
    print("  " + subprocess.list2cmdline([str(pio), "run", "--project-dir", str(ROOT / "esp32")]))
    print("To flash firmware, use the same command with: -t upload")
    print("To list serial ports:")
    print("  " + subprocess.list2cmdline([str(pio), "device", "list"]))
    print("To start the laptop bridge, set RFID_API_BASE_URL, RFID_DEVICE_ID, and")
    print("RFID_DEVICE_SECRET_HEX, then run:")
    print("  " + subprocess.list2cmdline([str(python), str(ROOT / "laptop_bridge.py"), "--port", "<your-port>"]))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"Installation stopped: command exited with status {error.returncode}", file=sys.stderr)
        raise SystemExit(error.returncode) from None
