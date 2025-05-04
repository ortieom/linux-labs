import logging
import sys
from pathlib import Path

import typer
from rich.console import Console
from rich.panel import Panel

from bldd.service.scanner import scan_directory
from bldd.service.reporter import generate_txt_report

logger = logging.getLogger(__name__)

app = typer.Typer(
    help="BLDD - Backward LDD",
    add_completion=False,
)
console = Console()


@app.command()
def scan(
    directory: Path = typer.Argument(
        ...,
        exists=True,
        file_okay=False,
        dir_okay=True,
        help="Directory to scan for executables.",
    ),
    libraries: list[str] = typer.Argument(
        ..., help="List of shared libraries to find in dependencies."
    ),
    output_filename: str = typer.Option(
        "bldd_scan_report.txt",
        "--output",
        "-o",
        help="Name of the produced report file.",
    ),
    recursive: bool = typer.Option(
        False, "--recursive", "-r", help="Scan directory recursively."
    ),
    verbose: bool = typer.Option(
        False,
        "--verbose",
        "-v",
        help="Enable verbose output.",
    ),
) -> None:
    """Scan a directory for executable files that have specified shared library dependencies."""
    if verbose:
        logger.setLevel(logging.DEBUG)

    # Require at least one library
    if not libraries:
        console.print("[bold red]Error: At least one library must be specified[/]")
        sys.exit(1)

    console.print(f"[bold blue]Scanning directory:[/] {directory.absolute()}")

    console.print(f"[bold blue]Selecting libraries:[/] {', '.join(libraries)}")

    if recursive:
        console.print("[bold]Scanning recursively[/]")

    try:
        with console.status("[bold green]Scanning for executables...[/]"):
            libs_to_execs = scan_directory(directory, libraries, recursive=recursive)
        if not libs_to_execs:
            console.print("[yellow]No executables found.[/]")
            return
        console.print(f"[bold green]Generating report to[/] {output_filename}")
        generate_txt_report(libs_to_execs, output_filename, directory)
        console.print(f"[bold green]Report successfully saved to[/] {output_filename}")
    except Exception as e:
        console.print(Panel(f"[bold red]Error: {str(e)}[/]", title="Error"))
        logger.exception("Error: %s", str(e))
        sys.exit(1)
