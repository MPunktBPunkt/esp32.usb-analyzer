Import("env")  # type: ignore  # PlatformIO
import subprocess
import sys
from pathlib import Path

proj = Path(env["PROJECT_DIR"])
script = proj / "tools" / "pack_ui.py"
rc = subprocess.call([sys.executable, str(script)], cwd=proj)
if rc != 0:
    sys.exit(rc)
