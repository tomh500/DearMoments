import re

class Directive:
    def __init__(self, kind, **kw):
        self.kind = kind
        self.__dict__.update(kw)

# 恢复严格匹配，确保不会误触 func 内部的选项
PAT_TEXT = re.compile(r'^text\s+"?([^"]*)"?\s*$')
PAT_FUNC = re.compile(r'^func\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]+)"\s+(\d+)')
PAT_OPT  = re.compile(r'^\s*"([^"]+)"\s+"([^"]+)"')
PAT_KEY  = re.compile(r'^key\s+"([^"]*)"\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]*)"')
PAT_LINE = re.compile(r'^line\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]*)"')

def parse_asul(path):
    print(f"--- 正在解析文件: {path} ---")
    try:
        with open(path, encoding='utf-8') as f:
            lines = f.read().splitlines()
    except Exception:
        return []

    ds, i = [], 0
    while i < len(lines):
        raw_line = lines[i]
        line = raw_line.strip()
        
        if not line:
            ds.append(Directive('raw', content=""))
            i += 1
            continue

        # 1. 解析 func
        if m := PAT_FUNC.match(line):
            title, detail, default_key, count_str = m.groups()
            expected_count = int(count_str)
            options = []
            scan_idx = i + 1
            
            while scan_idx < len(lines) and len(options) < expected_count:
                opt_raw = lines[scan_idx]
                if not opt_raw.strip():
                    scan_idx += 1
                    continue
                
                if opt_m := PAT_OPT.match(opt_raw):
                    options.append(opt_m.groups())
                scan_idx += 1
            
            ds.append(Directive('func', title=title, detail=detail, 
                                default_key=default_key, options=options))
            i = scan_idx
            continue

        # 2. 解析 key
        if m := PAT_KEY.match(line):
            ds.append(Directive('key', default_key=m.group(1), command=m.group(2), 
                                title=m.group(3), detail=m.group(4)))
            i += 1
            continue

        # 3. 解析 line
        if m := PAT_LINE.match(line):
            ds.append(Directive('line', title=m.group(1), detail=m.group(2), 
                                template=m.group(3), default=m.group(4)))
            i += 1
            continue

        # 4. 解析 text
        if m := PAT_TEXT.match(line):
            content = m.group(1)
            ds.append(Directive('text', content=content))
            i += 1
            continue

        # 兜底：当作原始文本
        ds.append(Directive('raw', content=raw_line))
        i += 1
        
    print(f"--- 解析完成，得到 {len(ds)} 个指令 ---")    
    return ds