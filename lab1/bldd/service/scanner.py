import concurrent.futures
from collections import defaultdict
import logging
import os
from typing import Dict, List, Optional

import lief

from bldd.domain.executable import ExecutableInfo

logger = logging.getLogger(__name__)


def scan_directory(
    directory: str,
    target_libraries: list[str],
    *,
    recursive: bool = True,
    max_workers: Optional[int] = None,
) -> Dict[str, List[ExecutableInfo]]:
    libs_to_execs: Dict[str, List[ExecutableInfo]] = defaultdict(list)

    file_paths = []

    for root, _, files in os.walk(directory):
        for filename in files:
            file_path = os.path.join(root, filename)
            file_paths.append(file_path)

        if not recursive:
            break

    with concurrent.futures.ThreadPoolExecutor(
        max_workers=max_workers or min(32, os.cpu_count() + 4)
    ) as executor:
        future_to_path = {}

        for file_path in file_paths:
            future = executor.submit(_analyze_file, file_path, target_libraries)
            future_to_path[future] = file_path

        for future in concurrent.futures.as_completed(future_to_path):
            file_path = future_to_path[future]

            try:
                result = future.result()
                if result:
                    for lib_name, exec_info in result.items():
                        libs_to_execs[lib_name].extend(exec_info)
            except Exception as e:
                logger.debug("Error analyzing %s: %s", str(file_path), str(e))

    return libs_to_execs


def _analyze_file(
    file_path: str, target_libraries: list[str]
) -> Dict[str, ExecutableInfo]:
    if not is_elf_file(file_path):
        return {}

    bin_info = lief.parse(str(file_path))

    if bin_info is None or not isinstance(bin_info, lief.ELF.Binary):
        return {}

    arch = bin_info.header.machine_type
    exec_info = ExecutableInfo(file_path, arch)

    result = defaultdict(list)
    if not bin_info.libraries:
        return {}

    for lib in bin_info.libraries:
        if lib not in target_libraries:
            continue

        result[lib].append(exec_info)

    return result


def is_elf_file(file_path):
    elf_magic_bytes = b"\x7fELF"

    try:
        with open(file_path, "rb") as f:
            magic_bytes = f.read(4)
            return magic_bytes == elf_magic_bytes

    except Exception as e:
        logger.debug("Error reading file %s: %s", file_path, str(e))
        return False
