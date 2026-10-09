# Spec 027 FR-010: every custom_sdkconfig option must land in the generated
# sdkconfig with the value we asked for. ESP-IDF renames options between
# releases; a renamed one is dropped silently by the generator, so this fails
# the build instead of shipping a different configuration.
import os
import re

Import("env")  # noqa: F821 (SCons)

ENV_NAME = env["PIOENV"]  # noqa: F821
PROJECT_DIR = env["PROJECT_DIR"]  # noqa: F821


def wanted_options():
    raw = env.GetProjectOption("custom_sdkconfig", "")  # noqa: F821
    options = {}
    for line in raw.splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        options[key.strip()] = value.strip()
    return options


def generated_files():
    names = [f"sdkconfig.{ENV_NAME}", "sdkconfig"]
    return [os.path.join(PROJECT_DIR, n) for n in names if os.path.isfile(os.path.join(PROJECT_DIR, n))]


def read_config(path):
    values = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            match = re.match(r"^(CONFIG_[A-Z0-9_]+)=(.*)$", line)
            if match:
                values[match.group(1)] = match.group(2)
                continue
            match = re.match(r"^# (CONFIG_[A-Z0-9_]+) is not set$", line)
            if match:
                values[match.group(1)] = "n"
    return values


def check(*_args, **_kwargs):
    wanted = wanted_options()
    if not wanted:
        return
    files = generated_files()
    if not files:
        print(f"check_sdkconfig: no generated sdkconfig for {ENV_NAME} in {PROJECT_DIR}")
        env.Exit(1)  # noqa: F821
    actual = read_config(files[0])
    missing = []
    for key, value in wanted.items():
        got = actual.get(key)
        if got is None and value == "n":
            continue  # not set is "n"
        if got != value:
            missing.append(f"{key}={value} (generated: {got if got is not None else 'absent'})")
    if missing:
        print(f"check_sdkconfig: {len(missing)} option(s) did not apply in {os.path.basename(files[0])}:")
        for item in missing:
            print(f"  {item}")
        env.Exit(1)  # noqa: F821
    print(f"check_sdkconfig: all {len(wanted)} options applied ({os.path.basename(files[0])})")


env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", check)  # noqa: F821
