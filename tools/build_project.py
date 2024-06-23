#!/usr/bin/env python3
"""Tool to retarget and build a SLCP project based on a manifest."""

from __future__ import annotations

import re
import ast
import sys
import copy
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

from xml.etree import ElementTree
from ruamel.yaml import YAML

# Matches all known chip and board names. Some varied examples:
#   MGM210PB32JIA EFM32GG890F512 MGM12P22F1024GA EZR32WG330F128R69 EFR32FG14P231F256GM32
CHIP_REGEX = re.compile(r"^(?:[a-z]+\d+){2,5}[a-z]*$", flags=re.IGNORECASE)
BOARD_REGEX = re.compile(r"^brd\d+[a-z](?:_.+)?$", flags=re.IGNORECASE)
GRAPH_COMPONENT_REGEX = re.compile(r"^(\w+)|- (\w+)", flags=re.MULTILINE)

CHIP_SPECIFIC_COMPONENTS = {
    "sl_system": "sl_system",
    "sl_memory": "sl_memory",
    # There are only a few components whose filename doesn't match the component name
    "freertos": "freertos_kernel",
}

SLC = ["slc", "--daemon", "--daemon-timeout", "1"]

LOGGER = logging.getLogger(__name__)


yaml = YAML(typ="safe")


def evaulate_f_string(f_string: str, variables: dict[str, typing.Any]) -> str:
    """
    Evaluates an `f`-string with the given locals.
    """

    return eval("f" + repr(f_string), variables)


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


def determine_chip_specific_config_filenames(
    slcp_path: pathlib.Path, sdk: pathlib.Path
) -> list[str]:
    """Determine the chip-specific config files to remove."""
    proc = subprocess.run(
        SLC
        + [
            "graph",
            "--project-file",
            str(slcp_path.absolute()),
            "--sdk",
            str(sdk.absolute()),
        ],
        cwd=str(slcp_path.parent),
        check=True,
        text=True,
        capture_output=True,
    )

    all_components = {a or b for a, b in GRAPH_COMPONENT_REGEX.findall(proc.stdout)}

    # Some configs don't seem to be present explicitly in the component graph
    config_files = {"sl_memory_config.h"}

    # List all of the SLCC files. There are neary 4,000.
    slcc_paths = {slcc.stem.lower(): slcc for slcc in sdk.glob("**/*.slcc")}

    for component_id in all_components:
        if CHIP_REGEX.match(component_id) or BOARD_REGEX.match(component_id):
            pass
        elif component_id in CHIP_SPECIFIC_COMPONENTS:
            component_id = CHIP_SPECIFIC_COMPONENTS[component_id]
        else:
            continue

        slcc = yaml.load(slcc_paths[component_id.lower()].read_text())

        for config_file in slcc.get("config_file", []):
            path = pathlib.PurePosixPath(config_file["path"]).name
            config_files.add(path)

    return config_files


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

    LOGGER.info("Building in %s", args.build_dir.resolve())

    # argparse defaults should be replaced, not extended
    if args.sdks != get_sdk_default_paths():
        args.sdks = args.sdks[len(get_sdk_default_paths()) :]

    if args.toolchains != get_toolchain_default_paths():
        args.toolchains = args.toolchains[len(get_toolchain_default_paths()) :]

    manifest = yaml.load(args.manifest.read_text())

    for key, override in args.overrides:
        manifest[key] = override

    # First, load the base project
    projects_root = pathlib.Path(__file__).parent.parent
    base_project_path = projects_root / manifest["base_project"]
    assert base_project_path.is_relative_to(projects_root)
    (base_project_slcp,) = base_project_path.glob("*.slcp")
    base_project = yaml.load(base_project_slcp.read_text())
    base_project_name = base_project_path.stem

    output_project = copy.deepcopy(base_project)

    # Strip chip- and board-specific components to modify the base device type
    output_project["component"] = [
        c
        for c in output_project["component"]
        if not CHIP_REGEX.match(c["id"]) and not BOARD_REGEX.match(c["id"])
    ]
    output_project["component"].append({"id": manifest["device"]})

    # Add new components
    output_project["component"].extend(manifest.get("add_components", []))
    output_project["toolchain_settings"].extend(manifest.get("toolchain_settings", []))

    # Remove components
    for component in manifest.get("remove_components", []):
        try:
            output_project["component"].remove(component)
        except ValueError:
            LOGGER.warning(
                "Component %s is not present in manifest, cannot remove", component
            )
            sys.exit(1)

    # Extend configuration and C defines
    for input_config, output_config in [
        (
            manifest.get("configuration", {}),
            output_project.setdefault("configuration", []),
        ),
        (
            manifest.get("slcp_defines", {}),
            output_project.setdefault("define", []),
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

    # Template variables for C defines
    value_template_env = {
        "git_repo_hash": get_git_commit_id(repo=pathlib.Path(__file__).parent.parent),
        "manifest_name": args.manifest.stem,
    }

    # Copy the base project into the output directory
    if args.clean_build_dir:
        with contextlib.suppress(OSError):
            shutil.rmtree(args.build_dir)

    args.build_dir.mkdir(exist_ok=True, parents=True)

    shutil.copytree(
        base_project_path,
        args.build_dir,
        dirs_exist_ok=True,
        ignore=lambda dir, contents: [
            # XXX: pruning `autogen` is extremely important!
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

    # Delete the original project file
    (args.build_dir / base_project_slcp.name).unlink()

    # Write the new project SLCP file
    with (args.build_dir / f"{base_project_name}.slcp").open("w") as f:
        yaml.dump(output_project, f)

    # Create a GBL metadata file
    with (args.build_dir / "gbl_metadata.yaml").open("w") as f:
        yaml.dump(manifest["gbl"], f)

    # Generate a build directory
    cmake_build_root = args.build_dir / f"{base_project_name}_cmake"
    shutil.rmtree(cmake_build_root, ignore_errors=True)

    sdk_name: str

    # Find the SDK version required by the project
    for sdk in args.sdks:
        sdk_name = "gecko_sdk" if "gecko_sdk" in sdk.name else "simplicity_sdk"

        try:
            sdk_meta = yaml.load((sdk / f"{sdk_name}.slcs").read_text())
        except FileNotFoundError:
            LOGGER.warning("SDK %s is not valid, skipping", sdk)
            continue

        LOGGER.info("SDK %s has version %s", sdk, sdk_meta["sdk_version"])

        if base_project["sdk"]["id"] != sdk_name:
            continue

        if base_project["sdk"]["version"] == sdk_meta["sdk_version"]:
            LOGGER.info("Version is correct, picking %s", sdk)
            break
    else:
        LOGGER.error("Project SDK version %s not found", base_project["sdk"]["version"])
        sys.exit(1)

    LOGGER.info("Building component graph and identifying board-specific files")
    for name in determine_chip_specific_config_filenames(
        slcp_path=base_project_slcp, sdk=sdk
    ):
        try:
            (args.build_dir / f"config/{name}").unlink()
        except FileNotFoundError:
            pass
        else:
            LOGGER.info("Deleted device-specific config: %s", name)

    # Find the toolchain required by the project
    slps_path = (args.build_dir / base_project_name).with_suffix(".slps")
    slps_xml = ElementTree.parse(slps_path)
    slps_toolchain_id = (
        slps_xml.getroot()
        .find(".//properties[@key='projectCommon.toolchainId']")
        .attrib["value"]
        .split(":")[-1]
    )

    # Find the correct toolchain
    for toolchain in args.toolchains:
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

        toolchain_id = version_info["basever"] + "." + version_info["datestamp"]

        LOGGER.info("Toolchain %s has version %s", toolchain, toolchain_id)

        if toolchain_id == slps_toolchain_id:
            LOGGER.info("Version is correct, picking %s", toolchain)
            break
    else:
        LOGGER.error("Project toolchain version %s not found", slps_toolchain_id)
        sys.exit(1)

    # Make sure all extensions are valid
    for sdk_extension in base_project.get("sdk_extension", []):
        expected_dir = sdk / f"extension/{sdk_extension['id']}_extension"

        if not expected_dir.is_dir():
            LOGGER.error("Referenced extension not present in SDK: %s", expected_dir)
            sys.exit(1)

    subprocess.run(
        SLC
        + [
            "generate",
            "--project-file",
            (args.build_dir / f"{base_project_name}.slcp").resolve(),
            "--export-destination",
            args.build_dir.resolve(),
            "--sdk",
            sdk,
            "--toolchain",
            "toolchain_gcc",
            "--output-type",
            args.build_system,
        ],
        check=True,
    )

    # Actually search for C defines within config
    unused_defines = set(manifest.get("c_defines", {}).keys())

    for config_root in [args.build_dir / "autogen", args.build_dir / "config"]:
        for config_f in config_root.glob("*.h"):
            config_h_lines = config_f.read_text().split("\n")
            written_config = {}
            new_config_h_lines = []

            for index, line in enumerate(config_h_lines):
                for define, value_template in manifest.get("c_defines", {}).items():
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
                            c["name"] == define
                            for c in output_project.get("define", [])
                        )
                        new_config_h_lines[index - 1] = "#if 1"
                    elif "#warning" in prev_line:
                        assert re.match(r'#warning ".*? not configured"', prev_line)
                        new_config_h_lines.pop(index - 1)

                    value_template = str(value_template)

                    if value_template.startswith("template:"):
                        value = value_template.replace("template:", "", 1).format(
                            **value_template_env
                        )
                    else:
                        value = value_template

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

    subprocess.run(
        [
            "make",
            "-C",
            args.build_dir,
            "-f",
            f"{base_project_name}.Makefile",
            f"-j{multiprocessing.cpu_count()}",
            f"ARM_GCC_DIR={toolchain}",
            f"POST_BUILD_EXE={args.postbuild}",
            "VERBOSE=1",
        ],
        check=True,
    )

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
