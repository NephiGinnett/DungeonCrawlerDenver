"""
Decode a base64 image out of a saved MCP tool-result file into a .png.

WHY THIS EXISTS
    EditorAppToolset.CaptureAssetImage / CaptureViewport return PNGs as base64
    inside JSON. Even a small thumbnail blows the tool-output token limit, so the
    harness spills the response to a file instead. This pulls the image back out
    so it can actually be looked at.

USAGE
    python Tools/decode_tool_image.py <tool-result.txt> <out.png>
"""

import base64
import re
import sys

src, dst = sys.argv[1], sys.argv[2]

raw = open(src, "r", encoding="utf-8", errors="ignore").read()

# Match the longest base64 blob; short ones are mime types and refPaths.
matches = re.findall(r'"data"\s*:\s*"([A-Za-z0-9+/=]{500,})"', raw)
if not matches:
    print("no base64 image found in", src)
    raise SystemExit(1)

blob = max(matches, key=len)
open(dst, "wb").write(base64.b64decode(blob))
print("WROTE", dst)
