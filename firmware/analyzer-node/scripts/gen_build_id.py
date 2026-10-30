Import("env")  # type: ignore  # PlatformIO
import subprocess
from pathlib import Path

proj = Path(env["PROJECT_DIR"])
inc = proj / "include"
inc.mkdir(parents=True, exist_ok=True)
out = inc / "build_id.h"

# Prefer repo root (firmware/analyzer-node → ../..) for git SHA.
git_cwd = proj
for candidate in (proj, proj.parent, proj.parent.parent):
    if (candidate / ".git").exists():
        git_cwd = candidate
        break

sha = "unknown"
try:
    sha = (
        subprocess.check_output(
            ["git", "rev-parse", "--short=12", "HEAD"],
            cwd=git_cwd,
            stderr=subprocess.DEVNULL,
        )
        .decode()
        .strip()
    )
except Exception:
    pass

out.write_text(
    f'#pragma once\n#ifdef GIT_SHA\n#undef GIT_SHA\n#endif\n#define GIT_SHA "{sha}"\n',
    encoding="utf-8",
)
print(f"[gen_build_id] GIT_SHA={sha}")
