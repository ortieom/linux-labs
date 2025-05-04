from typing import Dict, List
from collections import defaultdict

from bldd.domain.executable import ExecutableInfo


def generate_txt_report(
    libs_to_execs: Dict[str, List[ExecutableInfo]],
    output_file: str,
    scan_dir: str,
) -> None:
    with open(output_file, "w", encoding="utf-8") as file:
        file.write(
            f"Report on dynamic used libraries by ELF executables on {scan_dir}\n"
        )

        architectures = defaultdict(lambda: defaultdict(list))
        for library_name, executables in libs_to_execs.items():
            for exec_info in executables:
                architectures[exec_info.architecture][library_name].append(exec_info)

        for arch in sorted(architectures.keys(), key=lambda x: x.value):
            file.write(f"---------- {arch.name} ----------\n")

            sorted_libraries = sorted(
                architectures[arch].items(), key=lambda x: len(x[1]), reverse=True
            )

            for library_name, executables in sorted_libraries:
                file.write(f"{library_name} ({len(executables)} execs)\n")

                sorted_execs = sorted(executables, key=lambda x: x.path)
                for exec_info in sorted_execs:
                    file.write(f"\t-> {exec_info.path}\n")
