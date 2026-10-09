# Spec 027 FR-010: pioarduino keeps the last build's full generated config as
# sdkconfig.defaults (shared by every env) and sdkconfig.<env>. A later build
# with different custom_sdkconfig is generated on top of them, so an option
# removed from platformio.ini - or a choice set by the other build - silently
# survives. Start from the platform's own defaults whenever the env or its
# options change; an unchanged rebuild keeps the files (no framework rebuild).
import hashlib
import os

Import("env")  # noqa: F821 (SCons)

project_dir = env["PROJECT_DIR"]  # noqa: F821
env_name = env["PIOENV"]  # noqa: F821
options = env.GetProjectOption("custom_sdkconfig", "")  # noqa: F821
stamp = f"{env_name}:{hashlib.sha256(options.encode('utf-8')).hexdigest()}"
stamp_path = os.path.join(project_dir, ".pio", "sdkconfig-owner")

previous = None
if os.path.isfile(stamp_path):
    with open(stamp_path, encoding="utf-8") as handle:
        previous = handle.read().strip()

if previous != stamp:
    removed = []
    for name in os.listdir(project_dir):
        if name == "sdkconfig" or name.startswith("sdkconfig."):
            os.remove(os.path.join(project_dir, name))
            removed.append(name)
    if removed:
        print(f"clean_sdkconfig: {env_name} options changed; removed {', '.join(sorted(removed))}")
    os.makedirs(os.path.dirname(stamp_path), exist_ok=True)
    with open(stamp_path, "w", encoding="utf-8") as handle:
        handle.write(stamp)
