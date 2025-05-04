# BLDD - Backward LDD

A command-line tool that finds all executables that have specified shared library as their dependencies.

## Features

- Scans directories for ELF executable files
- Identifies architecture of executable files (x86, x86_64, ARM, AArch64, etc.) using LIEF
- Parses shared library dependencies using LIEF
- Generates formatted report sorted by usage frequency

## Installation

```bash
# From the project directory
poetry install
```

## Usage

```bash
# Basic usage (provide directory and libraries to scan for)
bldd scan /path/to/directory libname1 libname2

# Specify output file name
bldd scan /path/to/directory libname1 --output report.txt

# Enable recursive directory scanning
bldd scan /path/to/directory libname1 --recursive

# Show verbose output
bldd scan /path/to/directory libname1 --verbose

# Show help
bldd --help
bldd scan --help
```

## Project Structure

```
lab1/
├── bldd/
│   ├── cli/           # Command line interface
│   ├── domain/        # Domain models
│   ├── service/       # Service layer
│   └── main.py        # Entry point
└── pyproject.toml     # Project configuration
``` 