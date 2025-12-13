#!/usr/bin/env python3
import sys
import argparse

MAX_COL = 80

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
        return chr(b)
    else:
        return f"\\x{b:02x}"

def emit_c_string(data: bytes, name: str):
    print(f"static const char {name}[] =")

    col = 0
    line = '    "'

    for b in data:
        esc = escape_byte(b)

        # Force break on literal newline
        if esc == "\\n":
            line += esc + '"'
            print(line)
            line = '    "'
            col = 0
            continue

        # Wrap if exceeding column limit
        if col + len(esc) >= MAX_COL:
            line += '"'
            print(line)
            line = '    "'
            col = 0

        line += esc
        col += len(esc)

    if line != '    "':
        line += '"'
        print(line)

    print(";")
    print(f"static const size_t {name}_length = {len(data)};")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="Input file")
    ap.add_argument(
        "--name",
        default="output_txt_source",
        help="C variable name"
    )
    args = ap.parse_args()

    with open(args.input, "rb") as f:
        data = f.read()

    emit_c_string(data, args.name)

if __name__ == "__main__":
    main()
