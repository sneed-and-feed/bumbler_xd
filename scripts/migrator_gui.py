#!/usr/bin/env python3
"""
scripts/migrator_gui.py
=======================
Musician-Friendly Desktop Graphical User Interface (GUI) for Bumbler XD
Legacy Preset Migration.

Converts legacy synthesizer presets (.fxp, .fxb, .fst, .flp, .xml) from
Wasp & Wasp XT into JUCE APVTS-compatible Bumbler XD XML presets.

Features:
  - Dark synth-styled theme matching Bumbler XD hardware/software aesthetic.
  - Multi-file and recursive folder input selection.
  - Category override or intelligent automatic patch acoustic classification.
  - Real-time progress tracking with threaded background migration.
  - Detailed color-coded live conversion log console.
  - Quick system file manager integration ("Open Output Folder").
  - Zero external pip dependencies (pure Python standard library).
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import threading
import tkinter as tk
from dataclasses import dataclass, field
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from typing import Callable, List, Optional, Sequence, Tuple

# Ensure project root and scripts directory are in sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS_DIR = Path(__file__).resolve().parent
for p in (str(PROJECT_ROOT), str(SCRIPTS_DIR)):
    if p not in sys.path:
        sys.path.insert(0, p)

try:
    from scripts.import_wasp_presets import (
        ALL_55_PARAMS,
        ParsedPreset,
        export_preset_to_xml,
        parse_preset_file,
        sanitize_filename,
    )
except ImportError:
    from import_wasp_presets import (
        ALL_55_PARAMS,
        ParsedPreset,
        export_preset_to_xml,
        parse_preset_file,
        sanitize_filename,
    )


# =============================================================================
# Synth Theme Color Palette (Bumbler XD Dark / Wasp Yellow)
# =============================================================================

DARK_BG = "#181c22"          # Dark synth background
PANEL_BG = "#222730"         # Panel / card surface
PANEL_BORDER = "#2e3542"     # Subtle panel border outline
ACCENT_YELLOW = "#e5a912"    # Signature Wasp yellow accent
ACCENT_HOVER = "#f5bc28"     # Bright hover yellow
ACCENT_ACTIVE = "#cca010"    # Pressed / active yellow
ACCENT_MUTED = "#554316"     # Muted yellow for tags
TEXT_WHITE = "#e0e0e0"       # High-contrast light text
TEXT_MUTED = "#8892a0"       # Muted subtitle / helper text
TEXT_DARK = "#181c22"        # Dark text for yellow buttons
INPUT_BG = "#15181f"         # Dark input entry field
INPUT_FG = "#ffffff"         # Text inside input field
INPUT_BORDER = "#374151"     # Input field outline
BTN_SEC_BG = "#2c333f"       # Secondary button background
BTN_SEC_HOVER = "#394252"    # Secondary button hover
BTN_SEC_FG = "#e0e0e0"       # Secondary button text
LOG_BG = "#13161c"           # Console / log background
LOG_FG = "#d1d5db"           # Default log text
COLOR_SUCCESS = "#4ade80"    # Green for successful migration
COLOR_WARN = "#fbbf24"       # Amber for warnings
COLOR_ERROR = "#f87171"      # Red for parsing failures
COLOR_CYAN = "#38bdf8"       # Cyan for paths and links

SUPPORTED_EXTENSIONS = {".fxp", ".fxb", ".fst", ".flp", ".xml"}

CATEGORIES = (
    "Auto-detect",
    "Bass",
    "Lead",
    "Pad",
    "Pluck",
    "Percussion",
    "FX",
    "Synth",
)


# =============================================================================
# Headless Migration Core & Helper Functions
# =============================================================================

def get_default_output_dir() -> Path:
    """Returns the default Bumbler XD migrated presets directory."""
    return Path.home() / "Documents" / "Bumbler XD" / "Presets" / "Migrated"


def open_folder(path: Path | str) -> bool:
    """
    Opens directory in the platform's default file manager:
      - Windows: os.startfile
      - macOS: open
      - Linux / other: xdg-open
    """
    target = Path(path)
    if not target.exists():
        target.mkdir(parents=True, exist_ok=True)

    try:
        if sys.platform == "win32":
            os.startfile(str(target))
            return True
        elif sys.platform == "darwin":
            subprocess.run(["open", str(target)], check=True)
            return True
        else:
            subprocess.run(["xdg-open", str(target)], check=True)
            return True
    except Exception as exc:
        print(f"[ERROR] Could not open folder '{target}': {exc}", file=sys.stderr)
        return False


def find_preset_files(paths: Sequence[Path | str]) -> List[Path]:
    """
    Discovers all supported legacy preset files from a list of files or directories.
    Handles individual files directly and recursively traverses directories.
    """
    results: List[Path] = []
    seen = set()

    for p in paths:
        path = Path(p)
        if not path.exists():
            continue
        if path.is_file():
            if path.suffix.lower() in SUPPORTED_EXTENSIONS:
                resolved = path.resolve()
                if resolved not in seen:
                    results.append(path)
                    seen.add(resolved)
        elif path.is_dir():
            for child in sorted(path.rglob("*")):
                if child.is_file() and child.suffix.lower() in SUPPORTED_EXTENSIONS:
                    resolved = child.resolve()
                    if resolved not in seen:
                        results.append(child)
                        seen.add(resolved)

    return results


@dataclass
class MigrationStats:
    """Statistics and outcome data for a batch preset migration run."""
    total_files: int = 0
    total_presets: int = 0
    successful_files: int = 0
    failed_files: int = 0
    errors: List[Tuple[Path, str]] = field(default_factory=list)
    output_files: List[Path] = field(default_factory=list)


def run_migration(
    input_paths: Sequence[Path | str],
    output_dir: Path | str,
    category_override: Optional[str] = None,
    progress_callback: Optional[Callable[[int, int, str], None]] = None,
    log_callback: Optional[Callable[[str, str], None]] = None,
    cancel_event: Optional[threading.Event] = None,
) -> MigrationStats:
    """
    Executes preset migration headlessly over all identified files.

    Args:
        input_paths: List of file or directory paths.
        output_dir: Target root directory for converted XML presets.
        category_override: Optional category override; if None or "Auto-detect",
                           preset category heuristic auto-classification is used.
        progress_callback: Called with (current_index, total_count, status_text).
        log_callback: Called with (message_text, message_style_tag).
        cancel_event: Optional threading event to signal early cancellation.

    Returns:
        MigrationStats summarizing the operation results.
    """
    stats = MigrationStats()
    files = find_preset_files(input_paths)
    stats.total_files = len(files)

    target_root = Path(output_dir)
    target_root.mkdir(parents=True, exist_ok=True)

    if not files:
        if log_callback:
            log_callback("No supported legacy preset files (.fxp, .fxb, .fst, .flp, .xml) found.", "warn")
        return stats

    if log_callback:
        log_callback(f"Discovered {len(files)} legacy preset file(s) for migration.", "highlight")
        log_callback(f"Destination: {target_root}", "info")
        cat_msg = f"Category: Override to '{category_override}'" if category_override and category_override != "Auto-detect" else "Category: Auto-detect (intelligent heuristic)"
        log_callback(cat_msg, "info")

    used_names: set[str] = set()

    for idx, file_path in enumerate(files, start=1):
        if cancel_event and cancel_event.is_set():
            if log_callback:
                log_callback("Migration cancelled by user.", "warn")
            break

        file_display = file_path.name
        if progress_callback:
            progress_callback(idx, len(files), f"Processing ({idx}/{len(files)}): {file_display}")

        try:
            presets = parse_preset_file(file_path)
            if not presets:
                if log_callback:
                    log_callback(f"  [WARN] No presets extracted from '{file_display}'", "warn")
                stats.failed_files += 1
                stats.errors.append((file_path, "No presets recognized in file"))
                continue

            for preset in presets:
                if category_override and category_override != "Auto-detect":
                    preset.category = category_override

                # Ensure category directory exists
                cat_dir = target_root / preset.category
                cat_dir.mkdir(parents=True, exist_ok=True)

                base_name = sanitize_filename(preset.name)
                candidate_name = f"{base_name}.xml"
                disambig_idx = 2
                while f"{preset.category}/{candidate_name}" in used_names:
                    candidate_name = f"{base_name}_{disambig_idx}.xml"
                    disambig_idx += 1
                used_names.add(f"{preset.category}/{candidate_name}")

                out_file = cat_dir / candidate_name
                xml_content = export_preset_to_xml(preset)
                out_file.write_text(xml_content, encoding="utf-8")

                stats.output_files.append(out_file)
                stats.total_presets += 1

                rel_dest = f"{preset.category}/{candidate_name}"
                if log_callback:
                    log_callback(f"  [MIGRATED] '{preset.name}' ({preset.category}) -> {rel_dest}", "success")

            stats.successful_files += 1

        except Exception as exc:
            stats.failed_files += 1
            stats.errors.append((file_path, str(exc)))
            if log_callback:
                log_callback(f"  [ERROR] Failed to convert '{file_display}': {exc}", "error")

    if progress_callback:
        progress_callback(len(files), len(files), "Complete")

    return stats


# =============================================================================
# Tkinter Musician-Friendly GUI Application
# =============================================================================

class MigratorApp:
    """Tkinter Desktop GUI for the Bumbler XD Legacy Preset Migrator."""

    def __init__(self, root: tk.Tk, initial_inputs: Optional[Sequence[Path | str]] = None) -> None:
        self.root = root
        self.root.title("Bumbler XD — Legacy Preset Migrator")
        self.root.geometry("820x680")
        self.root.minsize(720, 560)
        self.root.configure(bg=DARK_BG)

        self.selected_inputs: List[Path] = []
        self.is_migrating = False

        self._init_variables(initial_inputs)
        self._apply_styles()
        self._build_ui()
        self._print_welcome_message()

        # Handle initial inputs if provided via CLI
        if initial_inputs:
            self.set_input_paths(initial_inputs)

    def _init_variables(self, initial_inputs: Optional[Sequence[Path | str]]) -> None:
        self.input_path_var = tk.StringVar(value="")
        self.output_dir_var = tk.StringVar(value=str(get_default_output_dir()))
        self.category_var = tk.StringVar(value="Auto-detect")
        self.status_var = tk.StringVar(value="Ready. Select legacy presets or a folder to begin.")

    def _apply_styles(self) -> None:
        """Applies ttk synth-themed styles for widgets."""
        self.style = ttk.Style(self.root)
        try:
            self.style.theme_use("clam")
        except tk.TclError:
            pass

        # Progressbar
        self.style.configure(
            "Wasp.Horizontal.TProgressbar",
            troughcolor=INPUT_BG,
            background=ACCENT_YELLOW,
            bordercolor=PANEL_BORDER,
            lightcolor=ACCENT_YELLOW,
            darkcolor=ACCENT_YELLOW,
            thickness=14,
        )

        # Combobox
        self.style.configure(
            "TCombobox",
            fieldbackground=INPUT_BG,
            background=PANEL_BG,
            foreground=TEXT_WHITE,
            darkcolor=PANEL_BORDER,
            lightcolor=PANEL_BORDER,
            arrowcolor=ACCENT_YELLOW,
            bordercolor=PANEL_BORDER,
            padding=5,
        )
        self.style.map(
            "TCombobox",
            fieldbackground=[("readonly", INPUT_BG)],
            selectbackground=[("readonly", PANEL_BG)],
            selectforeground=[("readonly", ACCENT_YELLOW)],
            background=[("readonly", PANEL_BG)],
        )

    def _build_ui(self) -> None:
        """Constructs the responsive synth layout."""
        # Top Header Bar
        header_frame = tk.Frame(self.root, bg=DARK_BG, pady=12, padx=20)
        header_frame.pack(fill=tk.X)

        title_badge_frame = tk.Frame(header_frame, bg=DARK_BG)
        title_badge_frame.pack(fill=tk.X)

        title_label = tk.Label(
            title_badge_frame,
            text="Bumbler XD — Legacy Preset Migrator",
            font=("Segoe UI", 15, "bold"),
            fg=ACCENT_YELLOW,
            bg=DARK_BG,
        )
        title_label.pack(side=tk.LEFT)

        badge_label = tk.Label(
            title_badge_frame,
            text="WASP BRIDGE",
            font=("Segoe UI", 8, "bold"),
            fg=ACCENT_YELLOW,
            bg="#2a220e",
            padx=7,
            pady=2,
            relief=tk.FLAT,
            highlightthickness=1,
            highlightbackground=ACCENT_MUTED,
        )
        badge_label.pack(side=tk.RIGHT)

        subtitle_label = tk.Label(
            header_frame,
            text="Wasp / Wasp XT (.fxp, .fxb, .fst, .flp) to Bumbler XD XML",
            font=("Segoe UI", 9),
            fg=TEXT_MUTED,
            bg=DARK_BG,
        )
        subtitle_label.pack(anchor=tk.W, pady=(2, 0))

        # Divider line
        divider = tk.Frame(self.root, bg=PANEL_BORDER, height=1)
        divider.pack(fill=tk.X, padx=20)

        # Main Card Panel
        main_card = tk.Frame(
            self.root,
            bg=PANEL_BG,
            relief=tk.FLAT,
            highlightthickness=1,
            highlightbackground=PANEL_BORDER,
        )
        main_card.pack(fill=tk.X, padx=20, pady=14)

        # 1. Input Selection Section
        input_section = tk.Frame(main_card, bg=PANEL_BG, padx=16, pady=10)
        input_section.pack(fill=tk.X)

        input_title = tk.Label(
            input_section,
            text="LEGACY PRESET SOURCE",
            font=("Segoe UI", 8, "bold"),
            fg=TEXT_MUTED,
            bg=PANEL_BG,
        )
        input_title.pack(anchor=tk.W)

        input_row = tk.Frame(input_section, bg=PANEL_BG)
        input_row.pack(fill=tk.X, pady=(4, 0))

        self.input_entry = tk.Entry(
            input_row,
            textvariable=self.input_path_var,
            font=("Segoe UI", 9),
            bg=INPUT_BG,
            fg=INPUT_FG,
            insertbackground=ACCENT_YELLOW,
            relief=tk.FLAT,
            highlightthickness=1,
            highlightbackground=INPUT_BORDER,
            highlightcolor=ACCENT_YELLOW,
        )
        self.input_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, ipady=4, padx=(0, 8))

        btn_browse_files = self._create_secondary_button(
            input_row,
            text="Browse Files...",
            command=self.browse_files,
        )
        btn_browse_files.pack(side=tk.LEFT, padx=(0, 6))

        btn_browse_folder = self._create_secondary_button(
            input_row,
            text="Browse Folder...",
            command=self.browse_folder,
        )
        btn_browse_folder.pack(side=tk.LEFT)

        # 2. Output Directory Selection Section
        output_section = tk.Frame(main_card, bg=PANEL_BG, padx=16, pady=8)
        output_section.pack(fill=tk.X)

        output_title = tk.Label(
            output_section,
            text="OUTPUT DIRECTORY",
            font=("Segoe UI", 8, "bold"),
            fg=TEXT_MUTED,
            bg=PANEL_BG,
        )
        output_title.pack(anchor=tk.W)

        output_row = tk.Frame(output_section, bg=PANEL_BG)
        output_row.pack(fill=tk.X, pady=(4, 0))

        self.output_entry = tk.Entry(
            output_row,
            textvariable=self.output_dir_var,
            font=("Segoe UI", 9),
            bg=INPUT_BG,
            fg=INPUT_FG,
            insertbackground=ACCENT_YELLOW,
            relief=tk.FLAT,
            highlightthickness=1,
            highlightbackground=INPUT_BORDER,
            highlightcolor=ACCENT_YELLOW,
        )
        self.output_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, ipady=4, padx=(0, 8))

        btn_browse_output = self._create_secondary_button(
            output_row,
            text="Browse...",
            command=self.browse_output_dir,
        )
        btn_browse_output.pack(side=tk.LEFT, padx=(0, 6))

        btn_reset_output = self._create_secondary_button(
            output_row,
            text="Default",
            command=self.reset_default_output_dir,
        )
        btn_reset_output.pack(side=tk.LEFT)

        # 3. Category & Options Section
        options_section = tk.Frame(main_card, bg=PANEL_BG, padx=16, pady=8)
        options_section.pack(fill=tk.X)

        options_row = tk.Frame(options_section, bg=PANEL_BG)
        options_row.pack(fill=tk.X)

        cat_label = tk.Label(
            options_row,
            text="Category Classification:",
            font=("Segoe UI", 9),
            fg=TEXT_WHITE,
            bg=PANEL_BG,
        )
        cat_label.pack(side=tk.LEFT, padx=(0, 8))

        self.category_combo = ttk.Combobox(
            options_row,
            textvariable=self.category_var,
            values=CATEGORIES,
            state="readonly",
            width=16,
        )
        self.category_combo.pack(side=tk.LEFT)

        cat_hint = tk.Label(
            options_row,
            text="(Auto-detect uses acoustic heuristics to organize into Bass, Lead, Pad, etc.)",
            font=("Segoe UI", 8),
            fg=TEXT_MUTED,
            bg=PANEL_BG,
        )
        cat_hint.pack(side=tk.LEFT, padx=(10, 0))

        # 4. Action Row & Progress Tracking
        action_section = tk.Frame(main_card, bg=PANEL_BG, padx=16, pady=12)
        action_section.pack(fill=tk.X)

        action_buttons_row = tk.Frame(action_section, bg=PANEL_BG)
        action_buttons_row.pack(fill=tk.X, pady=(0, 10))

        self.btn_migrate = self._create_primary_button(
            action_buttons_row,
            text="⚡ Migrate Presets",
            command=self.start_migration,
        )
        self.btn_migrate.pack(side=tk.LEFT, padx=(0, 10))

        self.btn_open_folder = self._create_secondary_button(
            action_buttons_row,
            text="📂 Open Output Folder",
            command=self.open_output_folder,
        )
        self.btn_open_folder.pack(side=tk.LEFT)

        # Progressbar
        self.progressbar = ttk.Progressbar(
            action_section,
            style="Wasp.Horizontal.TProgressbar",
            mode="determinate",
            maximum=100,
            value=0,
        )
        self.progressbar.pack(fill=tk.X, pady=(0, 6))

        # Status Label
        self.status_label = tk.Label(
            action_section,
            textvariable=self.status_var,
            font=("Segoe UI", 9),
            fg=TEXT_MUTED,
            bg=PANEL_BG,
        )
        self.status_label.pack(anchor=tk.W)

        # 5. Live Console Log Area
        log_frame = tk.Frame(self.root, bg=DARK_BG, padx=20)
        log_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 14))

        log_header = tk.Frame(log_frame, bg=DARK_BG)
        log_header.pack(fill=tk.X, pady=(0, 4))

        log_title = tk.Label(
            log_header,
            text="CONVERSION ACTIVITY LOG",
            font=("Segoe UI", 8, "bold"),
            fg=TEXT_MUTED,
            bg=DARK_BG,
        )
        log_title.pack(side=tk.LEFT)

        btn_clear_log = tk.Label(
            log_header,
            text="Clear Log",
            font=("Segoe UI", 8, "underline"),
            fg=TEXT_MUTED,
            bg=DARK_BG,
            cursor="hand2",
        )
        btn_clear_log.pack(side=tk.RIGHT)
        btn_clear_log.bind("<Button-1>", lambda e: self.clear_log())
        btn_clear_log.bind("<Enter>", lambda e: btn_clear_log.config(fg=TEXT_WHITE))
        btn_clear_log.bind("<Leave>", lambda e: btn_clear_log.config(fg=TEXT_MUTED))

        # Scrolled Text Box
        log_container = tk.Frame(
            log_frame,
            bg=LOG_BG,
            highlightthickness=1,
            highlightbackground=PANEL_BORDER,
        )
        log_container.pack(fill=tk.BOTH, expand=True)

        self.log_scrollbar = ttk.Scrollbar(log_container, orient=tk.VERTICAL)
        self.log_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.log_text = tk.Text(
            log_container,
            bg=LOG_BG,
            fg=LOG_FG,
            insertbackground=ACCENT_YELLOW,
            font=("Consolas", 9),
            wrap=tk.WORD,
            yscrollcommand=self.log_scrollbar.set,
            relief=tk.FLAT,
            padx=10,
            pady=8,
            state=tk.NORMAL,
        )
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.log_scrollbar.config(command=self.log_text.yview)

        # Configure Log Text Tags
        self.log_text.tag_config("info", foreground=LOG_FG)
        self.log_text.tag_config("highlight", foreground=ACCENT_YELLOW)
        self.log_text.tag_config("success", foreground=COLOR_SUCCESS)
        self.log_text.tag_config("warn", foreground=COLOR_WARN)
        self.log_text.tag_config("error", foreground=COLOR_ERROR)
        self.log_text.tag_config("cyan", foreground=COLOR_CYAN)
        self.log_text.tag_config("muted", foreground=TEXT_MUTED)

        # Make read-only
        self.log_text.config(state=tk.DISABLED)

    # -------------------------------------------------------------------------
    # Button Builders with Interactive Hover Effects
    # -------------------------------------------------------------------------

    def _create_primary_button(
        self,
        parent: tk.Widget,
        text: str,
        command: Callable[[], None]
    ) -> tk.Button:
        """Creates a primary styled button in Wasp Yellow with hover transition."""
        btn = tk.Button(
            parent,
            text=text,
            command=command,
            font=("Segoe UI", 9, "bold"),
            bg=ACCENT_YELLOW,
            fg=TEXT_DARK,
            activebackground=ACCENT_ACTIVE,
            activeforeground=TEXT_DARK,
            relief=tk.FLAT,
            padx=16,
            pady=5,
            cursor="hand2",
        )

        def on_enter(e: tk.Event) -> None:
            if str(btn["state"]) != "disabled":
                btn.config(bg=ACCENT_HOVER)

        def on_leave(e: tk.Event) -> None:
            if str(btn["state"]) != "disabled":
                btn.config(bg=ACCENT_YELLOW)

        btn.bind("<Enter>", on_enter)
        btn.bind("<Leave>", on_leave)
        return btn

    def _create_secondary_button(
        self,
        parent: tk.Widget,
        text: str,
        command: Callable[[], None]
    ) -> tk.Button:
        """Creates a secondary panel-styled button with hover transition."""
        btn = tk.Button(
            parent,
            text=text,
            command=command,
            font=("Segoe UI", 9),
            bg=BTN_SEC_BG,
            fg=BTN_SEC_FG,
            activebackground=BTN_SEC_HOVER,
            activeforeground=TEXT_WHITE,
            relief=tk.FLAT,
            padx=12,
            pady=4,
            cursor="hand2",
            highlightthickness=1,
            highlightbackground=PANEL_BORDER,
        )

        def on_enter(e: tk.Event) -> None:
            if str(btn["state"]) != "disabled":
                btn.config(bg=BTN_SEC_HOVER)

        def on_leave(e: tk.Event) -> None:
            if str(btn["state"]) != "disabled":
                btn.config(bg=BTN_SEC_BG)

        btn.bind("<Enter>", on_enter)
        btn.bind("<Leave>", on_leave)
        return btn

    # -------------------------------------------------------------------------
    # UI Interactions & Dialog Handlers
    # -------------------------------------------------------------------------

    def set_input_paths(self, paths: Sequence[Path | str]) -> None:
        """Programmatically sets the input selection."""
        self.selected_inputs = [Path(p) for p in paths]
        if len(self.selected_inputs) == 1:
            self.input_path_var.set(str(self.selected_inputs[0]))
        elif len(self.selected_inputs) > 1:
            self.input_path_var.set(f"{len(self.selected_inputs)} items selected ({self.selected_inputs[0].name}, ...)")
        else:
            self.input_path_var.set("")

    def browse_files(self) -> None:
        """File browser supporting .fxp, .fxb, .fst, .flp, and .xml files."""
        file_types = [
            ("Supported Presets (*.fxp, *.fxb, *.fst, *.flp, *.xml)", "*.fxp;*.fxb;*.fst;*.flp;*.xml"),
            ("VST 2.4 Presets & Banks (*.fxp, *.fxb)", "*.fxp;*.fxb"),
            ("FL Studio State Files (*.fst)", "*.fst"),
            ("FL Studio Projects (*.flp)", "*.flp"),
            ("JUCE APVTS XML Presets (*.xml)", "*.xml"),
            ("All Files (*.*)", "*.*"),
        ]
        files = filedialog.askopenfilenames(
            title="Select Legacy Preset Files to Migrate",
            filetypes=file_types,
        )
        if files:
            self.set_input_paths(files)
            self.log(f"Selected {len(files)} preset file(s) for migration.", "info")

    def browse_folder(self) -> None:
        """Folder browser for batch directory migration."""
        folder = filedialog.askdirectory(title="Select Folder Containing Legacy Presets")
        if folder:
            self.set_input_paths([folder])
            self.log(f"Selected folder: '{folder}'", "info")

    def browse_output_dir(self) -> None:
        """Folder browser for destination directory."""
        folder = filedialog.askdirectory(
            title="Select Output Directory for Bumbler XD Presets",
            initialdir=self.output_dir_var.get() or str(get_default_output_dir()),
        )
        if folder:
            self.output_dir_var.set(folder)

    def reset_default_output_dir(self) -> None:
        """Resets the output directory to standard Documents folder."""
        default_dir = str(get_default_output_dir())
        self.output_dir_var.set(default_dir)
        self.log(f"Reset output directory to default: {default_dir}", "muted")

    def open_output_folder(self) -> None:
        """Opens destination directory in Windows Explorer or OS file manager."""
        target = self.output_dir_var.get().strip() or str(get_default_output_dir())
        success = open_folder(target)
        if success:
            self.log(f"Opened output folder in file manager: {target}", "info")
        else:
            messagebox.showerror("Error", f"Could not open directory:\n{target}")

    def get_effective_input_paths(self) -> List[Path]:
        """Resolves input paths from either stored selection or manual entry."""
        entry_text = self.input_path_var.get().strip()
        if not entry_text:
            return []

        # If user selected items via browser and hasn't overwritten with manual path
        if self.selected_inputs and (entry_text.endswith("items selected") or entry_text.startswith(str(self.selected_inputs[0]))):
            return self.selected_inputs

        # Check if single path on disk
        p = Path(entry_text)
        if p.exists():
            return [p]

        # Check semicolon or comma separated paths
        parts = [Path(chunk.strip()) for chunk in entry_text.replace(";", ",").split(",") if chunk.strip()]
        valid = [pt for pt in parts if pt.exists()]
        if valid:
            return valid

        return self.selected_inputs

    # -------------------------------------------------------------------------
    # Threaded Migration Execution
    # -------------------------------------------------------------------------

    def start_migration(self) -> None:
        """Validates inputs and kicks off worker thread."""
        if self.is_migrating:
            return

        inputs = self.get_effective_input_paths()
        if not inputs:
            messagebox.showwarning(
                "No Preset Source Selected",
                "Please select one or more legacy preset files (.fxp, .fxb, .fst, .flp) or a directory to migrate.",
            )
            return

        output_dir_str = self.output_dir_var.get().strip()
        if not output_dir_str:
            output_dir_str = str(get_default_output_dir())
            self.output_dir_var.set(output_dir_str)

        output_dir = Path(output_dir_str)
        category = self.category_var.get()

        # UI state lock
        self.is_migrating = True
        self.btn_migrate.config(state=tk.DISABLED, bg=BTN_SEC_BG, fg=TEXT_MUTED)
        self.progressbar.config(value=0)
        self.status_var.set("Scanning for legacy presets...")
        self.log("-" * 65, "muted")
        self.log("Starting preset migration task...", "highlight")

        # Launch background worker
        worker = threading.Thread(
            target=self._migration_worker,
            args=(inputs, output_dir, category),
            daemon=True,
        )
        worker.start()

    def _migration_worker(
        self,
        inputs: List[Path],
        output_dir: Path,
        category: str,
    ) -> None:
        """Background thread executing the migration without stalling the UI."""
        def on_progress(curr: int, total: int, text: str) -> None:
            pct = int((curr / total) * 100) if total > 0 else 0
            self.root.after(0, lambda: self._update_progress_ui(pct, text))

        def on_log(msg: str, tag: str) -> None:
            self.root.after(0, lambda: self.log(msg, tag))

        try:
            stats = run_migration(
                input_paths=inputs,
                output_dir=output_dir,
                category_override=category,
                progress_callback=on_progress,
                log_callback=on_log,
            )
            self.root.after(0, lambda: self._on_migration_complete(stats, output_dir))
        except Exception as exc:
            self.root.after(0, lambda: self._on_migration_fatal_error(str(exc)))

    def _update_progress_ui(self, percent: int, text: str) -> None:
        self.progressbar.config(value=percent)
        self.status_var.set(text)

    def _on_migration_complete(self, stats: MigrationStats, output_dir: Path) -> None:
        """Handles post-migration UI re-enablement and user notifications."""
        self.is_migrating = False
        self.btn_migrate.config(state=tk.NORMAL, bg=ACCENT_YELLOW, fg=TEXT_DARK)
        self.progressbar.config(value=100)

        summary_line = (
            f"Finished: {stats.total_presets} preset(s) migrated from {stats.successful_files} file(s). "
            f"Errors: {stats.failed_files}."
        )
        self.status_var.set(summary_line)
        self.log(summary_line, "highlight")

        if stats.total_files == 0:
            messagebox.showwarning(
                "No Presets Found",
                "No supported legacy preset files (.fxp, .fxb, .fst, .flp, .xml) were discovered in the selected source.",
            )
            return

        if stats.failed_files > 0:
            err_preview = "\n".join(f"• {f.name}: {err}" for f, err in stats.errors[:5])
            if len(stats.errors) > 5:
                err_preview += f"\n... and {len(stats.errors) - 5} more."

            if stats.successful_files == 0:
                messagebox.showerror(
                    "Migration Failed",
                    f"All {stats.failed_files} file(s) failed during migration.\n\nError details:\n{err_preview}",
                )
            else:
                messagebox.showwarning(
                    "Migration Completed with Errors",
                    f"Successfully migrated {stats.total_presets} presets.\n\n"
                    f"{stats.failed_files} file(s) encountered corruption or parsing errors:\n{err_preview}",
                )
        else:
            messagebox.showinfo(
                "Migration Complete",
                f"Successfully migrated {stats.total_presets} preset(s) into:\n{output_dir}",
            )

    def _on_migration_fatal_error(self, err_msg: str) -> None:
        """Handles unexpected worker exceptions."""
        self.is_migrating = False
        self.btn_migrate.config(state=tk.NORMAL, bg=ACCENT_YELLOW, fg=TEXT_DARK)
        self.status_var.set(f"Error during migration: {err_msg}")
        self.log(f"[FATAL ERROR] {err_msg}", "error")
        messagebox.showerror("Fatal Migration Error", f"An unexpected error occurred:\n{err_msg}")

    # -------------------------------------------------------------------------
    # Logging Utilities
    # -------------------------------------------------------------------------

    def log(self, text: str, tag: str = "info") -> None:
        """Appends formatted text line to the live activity console."""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, text + "\n", tag)
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def clear_log(self) -> None:
        """Clears the console log window."""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.config(state=tk.DISABLED)
        self._print_welcome_message()

    def _print_welcome_message(self) -> None:
        self.log("======================================================================", "highlight")
        self.log(" Bumbler XD — Legacy Preset Migrator", "highlight")
        self.log(" Converts Wasp & Wasp XT patches to JUCE APVTS XML Presets", "info")
        self.log(" Supported: .fxp (VST 2.4), .fxb (Banks), .fst (FL State), .flp (Projects)", "muted")
        self.log("======================================================================", "highlight")


# =============================================================================
# Main Entry Point & Standalone Launcher
# =============================================================================

def launch_gui(initial_inputs: Optional[Sequence[Path | str]] = None) -> None:
    """Launches the Tkinter GUI desktop application."""
    root = tk.Tk()

    # Try setting dark titlebar on Windows 10/11
    if sys.platform == "win32":
        try:
            import ctypes
            DWMWA_USE_IMMERSIVE_DARK_MODE = 20
            set_window_attribute = ctypes.windll.dwmapi.DwmSetWindowAttribute
            hwnd = ctypes.windll.user32.GetParent(root.winfo_id())
            rendering_policy = DWMWA_USE_IMMERSIVE_DARK_MODE
            value = ctypes.c_int(2)
            set_window_attribute(hwnd, rendering_policy, ctypes.byref(value), ctypes.sizeof(value))
        except Exception:
            pass

    app = MigratorApp(root, initial_inputs=initial_inputs)
    root.mainloop()


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Bumbler XD Legacy Preset Migrator Desktop GUI",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "inputs",
        nargs="*",
        help="Optional initial legacy preset file(s) or folder to load into GUI",
    )
    args = parser.parse_args(argv)

    launch_gui(initial_inputs=args.inputs if args.inputs else None)
    return 0


if __name__ == "__main__":
    sys.exit(main())
