import re
import sys
from pathlib import Path

# 粗略规则：匹配 namespace 内的变量定义（无 extern）
VAR_DEF = re.compile(
    r'^\s*(?!extern)\s*(std::atomic<[^>]+>|HANDLE|HWND|Mix_Music\s*\*)\s+(\w+)\s*(=|\{)',
    re.MULTILINE
)

def scan_file(path):
    text = path.read_text(encoding='utf-8', errors='ignore')
    return VAR_DEF.findall(text)

def main(root):
    defs = {}
    for cpp in Path(root).rglob("*.cpp"):
        for typ, name, _ in scan_file(cpp):
            defs.setdefault(name, []).append(str(cpp))

    print("=== 多重定义变量 ===")
    for name, files in defs.items():
        if len(files) > 1:
            print(f"\n{name}:")
            for f in files:
                print("  ", f)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: python detect_global_defs.py <src_root>")
        sys.exit(1)
    main(sys.argv[1])
