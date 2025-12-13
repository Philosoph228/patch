#!/usr/bin/env python3
r"""
Generate a vtf_wrapper-style C compound literal from a file.

This file is safe to run on Windows (no problematic \x sequences in the
normal docstring because it's a raw string).

Usage:
    python3 generate_vtf_from_file.py input.patch
    python3 generate_vtf_from_file.py input.patch --path "./tests/data/naughty/diff.path"

If --path is omitted, the script uses the input argument itself as the .path value.
"""
import argparse
import sys
import os

MAX_COL = 80  # maximum characters per quoted string (not counting indentation)

def escape_byte(b: int) -> str:
    """Return C-escaped representation of a single byte."""
    if b == 0x5C:      # backslash
        return "\\\\"
    elif b == 0x22:    # double quote
        return "\\\""
    elif b == 0x0A:    # LF
        return "\\n"
    elif b == 0x0D:    # CR
        return "\\r"
    elif b == 0x09:    # TAB
        return "\\t"
    elif 0x20 <= b <= 0x7E:
        # printable ASCII (space .. ~)
        return chr(b)
    else:
        # use hex escape for others
        return f"\\x{b:02x}"

def escape_path_string(s: str) -> str:
    """Escape a Python str for placement in a C quoted literal (no splitting)."""
    out = []
    for ch in s:
        b = ord(ch)
        if b == 0x5C:
            out.append("\\\\")
        elif b == 0x22:
            out.append("\\\"")
        elif b == 0x09:
            out.append("\\t")
        elif b == 0x0A:
            out.append("\\n")
        elif b == 0x0D:
            out.append("\\r")
        elif 0x20 <= b <= 0x7E:
            out.append(ch)
        else:
            out.append(f"\\x{b:02x}")
    return "".join(out)

def emit_vtf_wrapper(input_bytes: bytes, path_literal: str, wrapper_type: str, max_col: int):
    indent_field = "    "
    indent_data = indent_field + "    "  # matches 8 spaces in your example
    # Header
    print(f"({wrapper_type}) {{")
    # Path
    esc_path = escape_path_string(path_literal)
    print(f"{indent_field}.path = \"{esc_path}\",")
    # Data label
    print(f"{indent_field}.data =")

    # Build and print data lines
    line_prefix = indent_data + '"'
    line = line_prefix
    col = 0  # number of characters inside the current quoted string (not counting quotes)

    printed_lines = []

    for b in input_bytes:
        esc = escape_byte(b)

        # If the byte is LF, we want to include "\n" then force a new quoted string on the next C source line.
        if esc == "\\n":
            line += esc + '"'
            printed_lines.append(line)
            line = line_prefix
            col = 0
            continue

        # If adding this escape would exceed max_col, close current quoted string and start new one
        if col + len(esc) > max_col:
            line += '"'
            printed_lines.append(line)
            line = line_prefix
            col = 0

        # append escaped text
        line += esc
        col += len(esc)

    # Finish last line if it has content
    if line != line_prefix:
        line += '"'
        printed_lines.append(line)

    # Print all data lines; append comma to last printed line
    if printed_lines:
        for i, pl in enumerate(printed_lines):
            if i == len(printed_lines) - 1:
                # last data line: append comma
                print(pl + ",")
            else:
                print(pl)
    else:
        # empty data -> print a single empty quoted string with comma
        print(line_prefix + '",')

    # Length field
    print(f"{indent_field}.length = {len(input_bytes)},")
    # Closing brace
    print("}")

def main():
    ap = argparse.ArgumentParser(description="Generate a vtf_wrapper-style C compound literal from a file.")
    ap.add_argument("input", help="Input file to embed (binary-safe).")
    ap.add_argument("--path", help="Value for .path (string literal). If omitted, uses the input argument value.")
    ap.add_argument("--wrapper-type", default="vtf_wrapper_t", help="Type used for the compound literal (default: vtf_wrapper).")
    ap.add_argument("--max-col", type=int, default=MAX_COL, help=f"Max characters per quoted string (default {MAX_COL}).")
    args = ap.parse_args()

    input_arg = args.input

    try:
        with open(input_arg, "rb") as f:
            data = f.read()
    except Exception as e:
        print("Error reading input file:", e, file=sys.stderr)
        sys.exit(2)

    # If --path not provided, default to the input argument (unchanged)
    path_value = args.path if args.path is not None else input_arg

    emit_vtf_wrapper(data, path_value, args.wrapper_type, args.max_col)

if __name__ == "__main__":
    main()
