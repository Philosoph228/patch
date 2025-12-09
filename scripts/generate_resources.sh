#!/bin/sh
set -eu

# Get the directory where this script lives
script_dir="$(cd "$(dirname "$0")" && pwd)"

# Go one level up → project_root
baseDir="$(dirname "$script_dir")"

svgDir="$baseDir/assets/svg"
pngDir="$baseDir/assets/png"
bmpDir="$baseDir/assets/bmp"
icoDir="$baseDir/assets/ico"
resDir="$baseDir/res"

mkdir -p "$bmpDir" "$pngDir" "$icoDir" "$resDir"

# -----------------------
# Helpers: parse SVG sizes
# -----------------------

# Convert a CSS length (100, 100px, 2.54cm, 1in, 72pt, 12pc, 25.4mm) to px (float)
unit_to_px() {
  val="$1"
  if [ -z "$val" ]; then
    printf "0\n"
    return
  fi

  # Use awk for robust float parsing & unit conversion (1in = 96px)
  echo "$val" | awk '
  {
    s=$0
    match(s, /^([0-9]+(\.[0-9]+)?)([a-zA-Z%]*)$/, a)
    if (a[1]=="") { print 0; exit }
    num = a[1]
    unit = a[3]
    if (unit=="" || unit=="px") { printf("%.6f\n", num) }
    else if (unit=="in") { printf("%.6f\n", num * 96) }
    else if (unit=="cm") { printf("%.6f\n", num * 96 / 2.54) }
    else if (unit=="mm") { printf("%.6f\n", num * 96 / 25.4) }
    else if (unit=="pt") { printf("%.6f\n", num * 96 / 72) }
    else if (unit=="pc") { printf("%.6f\n", num * 16) } # 1pc = 12pt -> 12*(96/72)=16
    else { printf("%.6f\n", num) } # fallback, best-effort
  }'
}

# Get SVG width in CSS px. Prefer viewBox width, otherwise parse width or height.
get_svg_width_px() {
  svgfile="$1"

  # try viewBox: viewBox="minx miny width height"
  vb=$(grep -o -m1 'viewBox="[^\"]*"' "$svgfile" 2>/dev/null | sed 's/viewBox="//;s/"$//')
  if [ -n "$vb" ]; then
    # extract third number (width)
    width=$(echo "$vb" | awk '{print $3}')
    if [ -n "$width" ]; then
      # viewBox user units -> treat as px
      printf "%s\n" "$width"
      return 0
    fi
  fi

  # fallback: width attribute
  wattr=$(grep -o -m1 'width="[^\"]*"' "$svgfile" 2>/dev/null | sed 's/width="//;s/"$//')
  if [ -n "$wattr" ]; then
    unit_to_px "$wattr"
    return 0
  fi

  # fallback: height attribute (assume square)
  hattr=$(grep -o -m1 'height="[^\"]*"' "$svgfile" 2>/dev/null | sed 's/height="//;s/"$//')
  if [ -n "$hattr" ]; then
    unit_to_px "$hattr"
    return 0
  fi

  # final fallback default
  printf "100\n"
}

# -----------------------
# PNG generation (density computed)
# -----------------------
# gen_png_with_density name size [oversample]
# - name: svg basename (without .svg)
# - size: desired final pixel size (integer)
# - oversample: factor (default 6). 3..8 is reasonable.
gen_png_with_density() {
  name="$1"
  size="$2"
  oversample="${3:-6}"
  src="$svgDir/$name.svg"
  out="$pngDir/$name@${size}px.png"

  if [ ! -f "$src" ]; then
    echo "Source SVG not found: $src" >&2
    return 1
  fi

  mkdir -p "$(dirname "$out")"

  # read svg width in CSS px
  svg_w="$(get_svg_width_px "$src")"
  # compute density = round(96 * size * oversample / svg_w)
  density=$(awk -v s="$size" -v f="$oversample" -v w="$svg_w" 'BEGIN { if (w==0) w=100; d=96 * s * f / w; printf("%d\n", (d<0?0:d+0.5)) }')

  if [ "$density" -lt 1 ]; then
    density=1
  fi

  # Render: rasterize at computed density, then downsample cleanly
  # We rasterize at 'density' DPI producing approximately (svg_w * density / 96) px wide,
  # which should be about size * oversample; then downsample to 'size'.
  convert -background none -density "$density" "$src" -filter Lanczos -resize "${size}x${size}" -unsharp 0x0.5+0.5+0.02 "$out"

  echo "Rendered $out (density=${density})"
  return 0
}

# -----------------------
# gen_icon (uses gen_png_with_density)
# -----------------------
DEFAULT_SIZES="16 24 32 48 64 72 96 128 256 512 1024"

gen_icon() {
  name="$1"
  shift || true

  if [ "$#" -gt 0 ]; then
    sizes="$*"
  else
    sizes="$DEFAULT_SIZES"
  fi

  png_paths=""
  for s in $sizes; do
    if gen_png_with_density "$name" "$s"; then
      png_paths="$png_paths $pngDir/$name@${s}px.png"
    else
      echo "Warning: failed to generate ${s}px for $name" >&2
    fi
  done

  png_paths="$(echo "$png_paths" | sed 's/^ *//; s/ *$//')"

  if [ -z "$png_paths" ]; then
    echo "No PNGs generated for '$name', skipping .ico creation." >&2
    return 1
  fi

  ico_out="$icoDir/$name.ico"
  # shellcheck disable=SC2086
  convert -background none $png_paths "$ico_out"

  cp "$ico_out" "$resDir/"
  echo "Generated: $ico_out -> copied to $resDir/"
}

# -----------------------
# gen_strip (uses gen_png_with_density for 16/24/32)
# -----------------------
gen_strip() {
  name="$1"

  # generate PNGs for strip sizes using density-based generator
  gen_png_with_density "$name" 16 || return 1
  gen_png_with_density "$name" 24 || return 1
  gen_png_with_density "$name" 32 || return 1

  convert -background none "$pngDir/$name@16px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$name@16px.bmp"
  convert -background none "$pngDir/$name@24px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$name@24px.bmp"
  convert -background none "$pngDir/$name@32px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$name@32px.bmp"
  cp "$bmpDir/$name@16px.bmp" "$resDir/"
  cp "$bmpDir/$name@24px.bmp" "$resDir/"
  cp "$bmpDir/$name@32px.bmp" "$resDir/"

  convert -background none "$pngDir/$name@16px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_hot@16px.bmp"
  convert -background none "$pngDir/$name@24px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_hot@24px.bmp"
  convert -background none "$pngDir/$name@32px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_hot@32px.bmp"
  cp "$bmpDir/${name}_hot@16px.bmp" "$resDir/"
  cp "$bmpDir/${name}_hot@24px.bmp" "$resDir/"
  cp "$bmpDir/${name}_hot@32px.bmp" "$resDir/"

  convert -background none "$pngDir/$name@16px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_disabled@16px.bmp"
  convert -background none "$pngDir/$name@24px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_disabled@24px.bmp"
  convert -background none "$pngDir/$name@32px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/${name}_disabled@32px.bmp"
  cp "$bmpDir/${name}_disabled@16px.bmp" "$resDir/"
  cp "$bmpDir/${name}_disabled@24px.bmp" "$resDir/"
  cp "$bmpDir/${name}_disabled@32px.bmp" "$resDir/"
}

# Example: generate for "patch"
gen_icon "patch"
# gen_strip "patch"   # uncomment if you want to generate strips too
