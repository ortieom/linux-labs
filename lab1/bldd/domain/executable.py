from dataclasses import dataclass


@dataclass
class ExecutableInfo:
    path: str
    architecture: str
