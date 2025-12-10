# Design

Running `patch.exe .\my_changes.patch` with no additional arguments should interpret the input and output filenames from the `---` and `+++` lines respectively.  
If the output path is the same as the input path, the patch will be applied **in-place** to the input file.

A unified diff may contain a concatenation of multiple diffs.  
In that case, each diff section must be processed sequentially from the beginning to the end of the patch file.

## Example

```diff
--- build_all.cmd     2025-12-09 23:02:33 +0500
+++ build_all2.cmd    2025-12-09 23:07:28 +0500
@@ -1,4 +1,4 @@
-REM Version 0.0.1
+REM Version 0.0.2

 echo off
 setlocal enabledelayedexpansion
--- build_openssl.bat     2025-12-09 23:02:33 +0500
+++ build_openssl2.bat    2025-12-09 23:07:28 +0500
@@ -1,4 +1,4 @@
-REM Version 0.0.1
+REM Version 0.0.2

 echo off
 setlocal enabledelayedexpansion
<EOF>
```

In this case `patch` should:

1. Apply the changes to `./build_all.cmd` and write the result to `./build_all2.cmd`
2. Apply the changes to `./build_openssl.bat` and write the result to `./build_openssl2.bat`

> [!CAUTION]
> If a file already exists at the output path, it **will be overwritten**.

#### `--apply-timestamp` flag

Sets the timestamp of the output file to match the timestamp specified next to the `+++` output filename.

#### `--force-inplace` flag

Ignores the `+++` output filename and writes all changes directly into the file from the`---` line (in-place).
