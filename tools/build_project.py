#!/usr/bin/env python3
"""Tool to retarget and build a SLCP project based on a manifest."""

from __future__ import annotations

import os
import re
import ast
import sys
import json
import time
import shutil
import typing
import hashlib
import logging
import pathlib
import argparse
import contextlib
import subprocess
import multiprocessing
from datetime import datetime, timezone

import jq
from ruamel.yaml import YAML


LOGGER = logging.getLogger(__name__)
SLC = ["slc", "--daemon", "--daemon-timeout", "1"]
DEFAULT_JSON_CONFIG = [
    # Fix a few paths by default
    {
        "file": "config/zcl/zcl_config.zap",
        "jq": '(.package[] | select(.type == "zcl-properties")).path = $zcl_zap',
        "args": {
            "zcl_zap": "template:{sdk}/app/zcl/zcl-zap.json",
        },
        "skip_if_missing": True,
    },
    {
        "file": "config/zcl/zcl_config.zap",
        "jq": '(.package[] | select(.type == "gen-templates-json")).path = $gen_templates',
        "args": {
            "gen_templates": "template:{sdk}/protocol/zigbee/app/framework/gen-template/gen-templates.json",
        },
        "skip_if_missing": True,
    },
]


yaml = YAML(typ="safe")


def evaulate_f_string(f_string: str, variables: dict[str, typing.Any]) -> str:
    """
    Evaluates an `f`-string with the given locals.
    """

    return eval("f" + repr(f_string), variables)


def expand_template(value: typing.Any, env: dict[str, typing.Any]) -> typing.Any:
    """Expand a template string."""
    if isinstance(value, str) and value.startswith("template:"):
        return evaulate_f_string(value.replace("template:", "", 1), env)
    else:
        return value


def ensure_folder(path: str | pathlib.Path) -> pathlib.Path:
    """Ensure that the path exists and is a folder."""
    path = pathlib.Path(path)

    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"Folder {path} does not exist")

    return path


def get_toolchain_default_paths() -> list[pathlib.Path]:
    """Return the path to the toolchain."""
    if sys.platform == "darwin":
        return list(
            pathlib.Path(
                "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/toolchains/gnu_arm/"
            ).glob("*")
        )

    return []


def get_sdk_default_paths() -> list[pathlib.Path]:
    """Return the path to the SDK."""
    if sys.platform == "darwin":
        return list(pathlib.Path("~/SimplicityStudio/SDKs").expanduser().glob("*_sdk*"))

    return []


def parse_override(override: str) -> tuple[str, dict | list]:
    """Parse a config override."""
    if "=" not in override:
        raise argparse.ArgumentTypeError("Override must be of the form `key=json`")

    key, value = override.split("=", 1)

    try:
        return key, json.loads(value)
    except json.JSONDecodeError as exc:
        raise argparse.ArgumentTypeError(f"Invalid JSON: {exc}")


def parse_prefixed_output(output: str) -> tuple[str, pathlib.Path | None]:
    """Parse a prefixed output parameter."""
    if ":" in output:
        prefix, _, path = output.partition(":")
        path = pathlib.Path(path)
    else:
        prefix = output
        path = None

    if prefix not in ("gbl", "hex", "out"):
        raise argparse.ArgumentTypeError(
            "Output format is of the form `gbl:overridden_filename.gbl` or just `gbl`"
        )

    return prefix, path


def get_git_commit_id(repo: pathlib.Path) -> str:
    """Get a commit hash for the current git repository."""

    def git(*args: str) -> str:
        result = subprocess.run(
            ["git", "-C", str(repo)] + list(args),
            text=True,
            capture_output=True,
            check=True,
        )
        return result.stdout.strip()

    # Get the current commit ID
    commit_id = git("rev-parse", "HEAD")[:8]

    # Check if the repository is dirty
    is_dirty = git("status", "--porcelain")

    # If dirty, append the SHA256 hash of the git diff to the commit ID
    if is_dirty:
        dirty_diff = git("diff")
        sha256_hash = hashlib.sha256(dirty_diff.encode()).hexdigest()[:8]
        commit_id += f"-dirty-{sha256_hash}"

    return commit_id


def load_sdks(paths: list[pathlib.Path]) -> dict[pathlib.Path, str]:
    """Load the SDK metadata from the SDKs."""
    sdks = {}

    for sdk in paths:
        sdk_file = next(sdk.glob("*_sdk.slcs"))

        try:
            sdk_meta = yaml.load(sdk_file.read_text())
        except FileNotFoundError:
            LOGGER.warning("SDK %s is not valid, skipping", sdk)
            continue

        sdk_id = sdk_meta["id"]
        sdk_version = sdk_meta["sdk_version"]
        sdks[sdk] = f"{sdk_id}:{sdk_version}"

    return sdks


def load_toolchains(paths: list[pathlib.Path]) -> dict[pathlib.Path, str]:
    """Load the toolchain metadata from the toolchains."""
    toolchains = {}

    for toolchain in paths:
        gcc_plugin_version_h = next(
            toolchain.glob("lib/gcc/arm-none-eabi/*/plugin/include/plugin-version.h")
        )
        version_info = {}

        for line in gcc_plugin_version_h.read_text().split("\n"):
            # static char basever[] = "10.3.1";
            if line.startswith("static char") and line.endswith(";"):
                name = line.split("[]", 1)[0].split()[-1]
                value = ast.literal_eval(line.split(" = ", 1)[1][:-1])
                version_info[name] = value

        toolchains[toolchain] = (
            version_info["basever"] + "." + version_info["datestamp"]
        )

    return toolchains


def subprocess_run_verbose(command: list[str], prefix: str, **kwargs) -> None:
    with subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs
    ) as proc:
        for line in proc.stdout:
            LOGGER.info("[%s] %r", prefix, line.decode("utf-8").strip())

    if proc.returncode != 0:
        LOGGER.error("[%s] Error: %s", prefix, proc.returncode)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--manifest",
        type=pathlib.Path,
        required=True,
        help="Firmware build manifest",
    )
    parser.add_argument(
        "--output",
        action="append",
        dest="outputs",
        type=parse_prefixed_output,
        required=True,
        help="Output file prefixed with its file type",
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        type=pathlib.Path,
        default=pathlib.Path("."),
        help="Output directory for artifacts, will be created if it does not exist",
    )
    parser.add_argument(
        "--no-clean-build-dir",
        action="store_false",
        dest="clean_build_dir",
        default=True,
        help="Do not clean the build directory",
    )
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        default=None,
        help="Temporary build directory, generated based on the manifest by default",
    )
    parser.add_argument(
        "--build-system",
        choices=["cmake", "makefile"],
        default="makefile",
        help="Build system",
    )
    parser.add_argument(
        "--sdk",
        action="append",
        dest="sdks",
        type=ensure_folder,
        default=get_sdk_default_paths(),
        required=len(get_sdk_default_paths()) == 0,
        help="Path to a Gecko SDK",
    )
    parser.add_argument(
        "--toolchain",
        action="append",
        dest="toolchains",
        type=ensure_folder,
        default=get_toolchain_default_paths(),
        required=len(get_toolchain_default_paths()) == 0,
        help="Path to a GCC toolchain",
    )
    parser.add_argument(
        "--postbuild",
        default=pathlib.Path(__file__).parent / "create_gbl.py",
        required=False,
        help="Postbuild executable",
    )
    parser.add_argument(
        "--override",
        action="append",
        dest="overrides",
        required=False,
        type=parse_override,
        default=[],
        help="Override config key with JSON.",
    )
    parser.add_argument(
        "--keep-slc-daemon",
        action="store_true",
        dest="keep_slc_daemon",
        default=False,
        help="Do not shut down the SLC daemon after the build",
    )

    args = parser.parse_args()

    if args.build_system != "makefile":
        LOGGER.warning("Only the `makefile` build system is currently supported")
        args.build_system = "makefile"

    if args.build_dir is None:
        args.build_dir = pathlib.Path(f"build/{time.time():.0f}_{args.manifest.stem}")

    # argparse defaults should be replaced, not extended
    if args.sdks != get_sdk_default_paths():
        args.sdks = args.sdks[len(get_sdk_default_paths()) :]

    if args.toolchains != get_toolchain_default_paths():
        args.toolchains = args.toolchains[len(get_toolchain_default_paths()) :]

    manifest = yaml.load(args.manifest.read_text())

    # Ensure we can load the correct SDK and toolchain
    sdks = load_sdks(args.sdks)
    sdk, sdk_name_version = next(
        (path, version) for path, version in sdks.items() if version == manifest["sdk"]
    )
    sdk_name, _, sdk_version = sdk_name_version.partition(":")

    toolchains = load_toolchains(args.toolchains)
    toolchain = next(
        path for path, version in toolchains.items() if version == manifest["toolchain"]
    )

    for key, override in args.overrides:
        manifest[key] = override

    # First, copy the base project into the build dir, under `template/`
    projects_root = pathlib.Path(__file__).parent.parent
    base_project_path = projects_root / manifest["base_project"]
    assert base_project_path.is_relative_to(projects_root)

    build_template_path = args.build_dir / "template"

    LOGGER.info("Building in %s", args.build_dir.resolve())

    if args.clean_build_dir:
        with contextlib.suppress(OSError):
            shutil.rmtree(args.build_dir)

    shutil.copytree(
        base_project_path,
        build_template_path,
        dirs_exist_ok=True,
        ignore=lambda dir, contents: [
            "autogen",
            ".git",
            ".settings",
            ".projectlinkstore",
            ".project",
            ".pdm",
            ".cproject",
            ".uceditor",
        ],
    )

    # We extend the base project with the manifest, since added components could have
    # extra dependencies
    (base_project_slcp,) = build_template_path.glob("*.slcp")
    base_project_name = base_project_slcp.stem
    base_project = yaml.load(base_project_slcp.read_text())

    # Add new components
    base_project["component"].extend(manifest.get("add_components", []))
    base_project.setdefault("toolchain_settings", []).extend(
        manifest.get("toolchain_settings", [])
    )

    # Remove components
    for component in manifest.get("remove_components", []):
        try:
            base_project["component"].remove(component)
        except ValueError:
            LOGGER.warning(
                "Component %s is not present in manifest, cannot remove", component
            )
            sys.exit(1)

    # Extend configuration and C defines
    for input_config, output_config in [
        (
            manifest.get("configuration", {}),
            base_project.setdefault("configuration", []),
        ),
        (
            manifest.get("slcp_defines", {}),
            base_project.setdefault("define", []),
        ),
    ]:
        for name, value in input_config.items():
            # Values are always strings
            value = str(value)

            # First try to replace any existing config entries
            for config in output_config:
                if config["name"] == name:
                    config["value"] = value
                    break
            else:
                # Otherwise, append it
                output_config.append({"name": name, "value": value})

    # Finally, write out the modified base project
    with base_project_slcp.open("w") as f:
        yaml.dump(base_project, f)

    # Create a GBL metadata file
    with (args.build_dir / "gbl_metadata.yaml").open("w") as f:
        yaml.dump(manifest["gbl"], f)

    # Template variables
    value_template_env = {
        "git_repo_hash": get_git_commit_id(repo=pathlib.Path(__file__).parent.parent),
        "manifest_name": args.manifest.stem,
        "now": datetime.now(timezone.utc),
        "sdk": sdk,
        "sdk_version": sdk_version,
    }

    for json_config in DEFAULT_JSON_CONFIG + manifest.get("json_config", []):
        json_path = build_template_path / json_config["file"]

        if json_config.get("skip_if_missing", False) and not json_path.exists():
            continue

        jq_args = {
            k: expand_template(v, value_template_env)
            for k, v in json_config.get("args", {}).items()
        }

        LOGGER.debug("Substituting JQ args for %s: %s", json_path, jq_args)

        result = (
            jq.compile(json_config["jq"], args=jq_args)
            .input_text(json_path.read_text())
            .first()
        )
        json_path.write_text(json.dumps(result, indent=2))

    # Next, generate a chip-specific project from the modified base project
    LOGGER.info(f"Generating project for {manifest['device']}")

    # fmt: off
    subprocess_run_verbose(
        SLC
        + [
            "generate",
            "--with", manifest["device"],
            "--project-file", base_project_slcp.resolve(),
            "--export-destination", args.build_dir.resolve(),
            "--copy-proj-sources",
            "--new-project",
            "--toolchain", "toolchain_gcc",
            "--sdk", sdk,
            "--output-type", args.build_system,
        ],
        "slc generate"
    )
    # fmt: on

    # Make sure all extensions are valid
    for sdk_extension in base_project.get("sdk_extension", []):
        expected_dir = sdk / f"extension/{sdk_extension['id']}_extension"

        if not expected_dir.is_dir():
            LOGGER.error("Referenced extension not present in SDK: %s", expected_dir)
            sys.exit(1)

    # Actually search for C defines within config
    unused_defines = set(manifest.get("c_defines", {}).keys())

    for config_root in [args.build_dir / "autogen", args.build_dir / "config"]:
        for config_f in config_root.glob("*.h"):
            config_h_lines = config_f.read_text().split("\n")
            written_config = {}
            new_config_h_lines = []

            for index, line in enumerate(config_h_lines):
                for define, value in manifest.get("c_defines", {}).items():
                    if f"#define {define} " not in line:
                        continue

                    define_with_whitespace = line.split(f"#define {define}", 1)[1]
                    alignment = define_with_whitespace[
                        : define_with_whitespace.index(define_with_whitespace.strip())
                    ]

                    prev_line = config_h_lines[index - 1]
                    if "#ifndef" in prev_line:
                        assert (
                            re.match(r"#ifndef\s+([A-Z0-9_]+)", prev_line).group(1)
                            == define
                        )

                        # Make sure that we do not have conflicting defines provided over the command line
                        assert not any(
                            c["name"] == define for c in base_project.get("define", [])
                        )
                        new_config_h_lines[index - 1] = "#if 1"
                    elif "#warning" in prev_line:
                        assert re.match(r'#warning ".*? not configured"', prev_line)
                        new_config_h_lines.pop(index - 1)

                    value = expand_template(value, value_template_env)
                    new_config_h_lines.append(f"#define {define}{alignment}{value}")
                    written_config[define] = value

                    if define not in unused_defines:
                        LOGGER.error("Define %r used twice!", define)
                        sys.exit(1)

                    unused_defines.remove(define)
                    break
                else:
                    new_config_h_lines.append(line)

            if written_config:
                LOGGER.info("Patching %s with %s", config_f, written_config)
                config_f.write_text("\n".join(new_config_h_lines))

    if unused_defines:
        LOGGER.error("Defines were unused, aborting: %s", unused_defines)
        sys.exit(1)

    # Fix Gecko SDK bugs
    sl_rail_util_pti_config_h = args.build_dir / "config/sl_rail_util_pti_config.h"

    # PTI seemingly cannot be excluded, even if it is disabled
    if sl_rail_util_pti_config_h.exists():
        sl_rail_util_pti_config_h.write_text(
            sl_rail_util_pti_config_h.read_text().replace(
                '#warning "RAIL PTI peripheral not configured"\n',
                '// #warning "RAIL PTI peripheral not configured"\n',
            )
        )

    # Remove absolute paths from the build for reproducibility
    extra_compiler_flags = [
        f"-ffile-prefix-map={str(src.absolute())}={dst}"
        for src, dst in {
            sdk: f"/{sdk_name}",
            args.build_dir: "/src",
            toolchain: "/toolchain",
        }.items()
    ] + ["-Wall", "-Wextra", "-Werror"]

    output_artifact = (args.build_dir / "build/debug" / base_project_name).with_suffix(
        ".gbl"
    )

    makefile = args.build_dir / f"{base_project_name}.Makefile"
    makefile_contents = makefile.read_text()

    # Inject a postbuild step into the makefile
    makefile_contents += "\n"
    makefile_contents += "post-build:\n"
    makefile_contents += (
        f"\t-{args.postbuild}"
        f' postbuild "{(args.build_dir / base_project_name).resolve()}.slpb"'
        f' --parameter build_dir:"{output_artifact.parent.resolve()}"'
        f' --parameter sdk_dir:"{sdk}"'
        "\n"
    )
    makefile_contents += "\t-@echo ' '"

    for flag in ("C_FLAGS", "CXX_FLAGS"):
        line = f"{flag:<17} = \n"
        suffix = " ".join([f'"{m}"' for m in extra_compiler_flags]) + "\n"
        makefile_contents = makefile_contents.replace(
            line, f"{line.rstrip()} {suffix}\n"
        )

    makefile.write_text(makefile_contents)

    # fmt: off
    subprocess_run_verbose(
        [   
            "make",
            "-C", args.build_dir,
            "-f", f"{base_project_name}.Makefile",
            f"-j{multiprocessing.cpu_count()}",
            f"ARM_GCC_DIR={toolchain}",
            f"POST_BUILD_EXE={args.postbuild}",
            "VERBOSE=1",
        ],
        "make",
        env={
            "PATH": f"{pathlib.Path(sys.executable).parent}:{os.environ['PATH']}"
        }
    )
    # fmt: on

    # Read the metadata extracted from the source and build trees
    extracted_gbl_metadata = json.loads(
        (output_artifact.parent / "gbl_metadata.json").read_text()
    )
    base_filename = evaulate_f_string(
        manifest.get("filename", "{manifest_name}"),
        {**value_template_env, **extracted_gbl_metadata},
    )

    args.output_dir.mkdir(exist_ok=True)

    # Copy the output artifacts
    for extension, output_path in args.outputs:
        if output_path is None:
            output_path = f"{base_filename}.{extension}"

        shutil.copy(
            src=output_artifact.with_suffix(f".{extension}"),
            dst=args.output_dir / output_path,
        )

    if args.clean_build_dir:
        with contextlib.suppress(OSError):
            shutil.rmtree(args.build_dir)

    if not args.keep_slc_daemon:
        subprocess.run(SLC + ["daemon-shutdown"], check=True)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
