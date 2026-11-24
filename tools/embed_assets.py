import sys
from pathlib import Path
from PIL import Image

gif_path = Path(sys.argv[1])
font_path = Path(sys.argv[2])
out_path = Path(sys.argv[3])

def emit_array(name, data):
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines)

frames = []
with Image.open(gif_path) as gif:
    for index in range(getattr(gif, "n_frames", 1)):
        gif.seek(index)
        duration = max(20, int(gif.info.get("duration", 80)))
        frame = gif.convert("RGBA")
        from io import BytesIO
        buffer = BytesIO()
        frame.save(buffer, format="PNG", optimize=True)
        frames.append((buffer.getvalue(), duration))

parts = [
    "#pragma once",
    "#include <cstddef>",
    "struct EmbeddedFrame { const unsigned char* data; std::size_t size; unsigned int duration; };",
]
for index, (data, duration) in enumerate(frames):
    parts.append(emit_array(f"kSplashFrame{index}", data))
parts.append(emit_array("kForgeFont", font_path.read_bytes()))
parts.append("static const EmbeddedFrame kSplashFrames[] = {")
for index, (data, duration) in enumerate(frames):
    parts.append(f"    {{ kSplashFrame{index}, sizeof(kSplashFrame{index}), {duration} }},")
parts.append("};")
parts.append(f"static constexpr std::size_t kSplashFrameCount = {len(frames)};")
out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
print(f"Embedded {len(frames)} PNG frames and {font_path.stat().st_size} font bytes.")
