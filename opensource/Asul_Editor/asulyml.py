import re, yaml, os, shutil, time 
from asulparser import Directive

# DSL 匹配正则
PAT_YML_TEXT  = re.compile(r'^text\s+"([^"]+)"\s+"([^"]+)"\s*$')
PAT_YML_INT   = re.compile(r'^int\s+"([^"]+)"\s+"(-?\d+),(-?\d+),(-?\d+)"\s+"([^"]+)"\s*$')
PAT_YML_BOOL  = re.compile(r'^bool\s+"([^"]+)"\s+"(true|false)"\s+"([^"]+)"\s*$')
PAT_YML_LINE  = re.compile(r'^line\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]+)"\s+"([^"]+)"\s*$')

def parse_asulyml(path):
    """解析 .asulyml，转换为 Directive 对象列表"""
    if not os.path.exists(path): return []
    lines = open(path, encoding='utf-8').read().splitlines()
    ds = []
    
    # 用于暂存 text 指令的内容，绑定给下一个真实的指令
    last_text_title = None
    last_text_detail = None

    for line in lines:
        line = line.strip()
        if not line or line.startswith('#'): continue

        # 1. 解析 text (作为标题和提示)
        # 匹配格式: text "标题" "详细说明"
        if m := re.match(r'^text\s+"([^"]*)"\s+"([^"]*)"', line):
            last_text_title, last_text_detail = m.groups()
            # 同时也作为一个纯文本显示行加入列表
            ds.append(Directive('text', content=last_text_title, detail=last_text_detail))
            continue

        # 2. 解析 int
        # 匹配格式: int "name" "min,max,def" "path"
        if m := re.match(r'^int\s+"([^"]+)"\s+"(-?\d+),(-?\d+),(-?\d+)"\s+"([^"]+)"', line):
            name, mn, mx, df, loc = m.groups()
            path_seg = [s for s in loc.split('/') if s]
            # 如果前面有 text 指令，就把 text 的内容给这个 int
            title = last_text_title if last_text_title else name
            detail = last_text_detail if last_text_detail else ""
            
            ds.append(Directive('int', name=name, title=title, detail=detail,
                              default=int(df), minimum=int(mn), maximum=int(mx), path=path_seg))
            # 用完重置
            last_text_title, last_text_detail = None, None
            continue

        # 3. 解析 line (数组/列表)
        # 匹配格式支持 4 或 5 个参数: line "name" "detail" "template" "default" "path"
        # 或者你现在的: line "name" "title" "default" "path"
        parts = re.findall(r'"([^"]*)"', line)
        if line.startswith('line') and len(parts) >= 4:
            name = parts[0]
            # 逻辑：如果是 4 个参数，取 [0]name, [1]detail, [2]default, [3]path
            # 如果是 5 个参数，取 [0]name, [1]detail, [2]template, [3]default, [4]path
            if len(parts) == 4:
                detail, template, default, loc = parts[1], "%1", parts[2], parts[3]
            else:
                detail, template, default, loc = parts[1], parts[2], parts[3], parts[4]
            
            path_seg = [s for s in loc.split('/') if s]
            title = last_text_title if last_text_title else name
            
            ds.append(Directive('line', name=name, title=title, detail=detail, 
                              template=template, default=default, path=path_seg))
            last_text_title, last_text_detail = None, None
            continue
            
    return ds


def save_asulyml(path, directives):
    """
    更新/保存 .asulyml 源码文件 (DSL 格式)
    对应 main.py 中的 save_asuljson 或 save_asul 逻辑
    """
    lines = []
    for d in directives:
        kind = getattr(d, 'kind', None)
        if kind == 'text':
            lines.append(f'text "{getattr(d, "content", "")}" "{getattr(d, "detail", "")}"')
        elif kind == 'int':
            loc = '/' + '/'.join(getattr(d, 'path', [])) if getattr(d, 'path', None) else "/"
            # 获取当前 UI 上的值
            val = d.var.get() if hasattr(d, 'var') else getattr(d, 'default', 0)
            lines.append(f'int "{d.name}" "{d.minimum},{d.maximum},{int(val)}" "{loc}"')
        elif kind == 'line':
            loc = '/' + '/'.join(getattr(d, 'path', [])) if getattr(d, 'path', None) else "/"
            val = d.var.get() if hasattr(d, 'var') else getattr(d, 'default', "")
            # 这里的格式要对应你 parse_asulyml 里的 5 参数逻辑
            lines.append(f'line "{d.name}" "{d.detail}" "{getattr(d, "template", "%1")}" "{val}" "{loc}"')
        else:
            # 原始行或不支持的类型直接写回
            lines.append(getattr(d, 'content', '') if hasattr(d, 'content') else "")

    # 源码也要备份
    folder = os.path.dirname(path)
    bak_dir = os.path.join(folder, 'backup')
    if os.path.isfile(path):
        os.makedirs(bak_dir, exist_ok=True)
        ts = time.strftime("%Y%m%d_%H%M%S")
        shutil.copy2(path, os.path.join(bak_dir, f"{os.path.basename(path)}.{ts}.bak"))

    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write("\n".join(lines) + "\n")
        

def write_yml(out_path, directives):
    """写出标准带注释的 YAML"""
    lines = []
    
    # 模拟一个层级结构写入
    # 为了保证 100% 保留注释且格式漂亮，我们采用手动构建行的方式
    current_path = []
    folder = os.path.dirname(out_path)
    backup_dir = os.path.join(folder, 'backup')
    os.makedirs(folder, exist_ok=True)
    # 如果旧文件存在，执行备份
    if os.path.isfile(out_path):
        os.makedirs(backup_dir, exist_ok=True)
        base = os.path.basename(out_path)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        # 备份格式：文件名.20260121_180000.bak
        shutil.copy2(out_path, os.path.join(backup_dir, f"{base}.{timestamp}.bak"))


    for d in directives:
        if d.kind == 'text':
            # 将 text 指令转为 YAML 注释
            content = d.content.strip()
            if content:
                # 保持一点缩进感，简单处理：如果 text 本身带空格就保留
                lines.append(f"# {content}")
            else:
                lines.append("")
            continue

        # 处理路径缩进
        target_path = getattr(d, 'path', [])
        
        # 写入层级 Key (例如 quick_stop: \n  logic:)
        for i, seg in enumerate(target_path):
            if i >= len(current_path) or current_path[i] != seg:
                indent = "  " * i
                lines.append(f"{indent}{seg}:")
                # 更新当前路径，后面的部分失效
                current_path = target_path[:i+1]

        # 写入具体的键值对
        indent = "  " * len(target_path)
        val = d.var.get() if hasattr(d, 'var') else d.default
        
        # 处理 int 类型
        if d.kind == 'int':
            lines.append(f"{indent}{d.name}: {int(val)}")
        # 处理 bool 类型
        elif d.kind == 'bool':
            lines.append(f"{indent}{d.name}: {str(val).lower()}")
        # 处理 line 类型 (重点：处理数组)
        elif d.kind == 'line':
            # 如果输入的是 [160, 162] 这种，直接写不加引号
            if str(val).startswith('[') and str(val).endswith(']'):
                lines.append(f"{indent}{d.name}: {val}")
            else:
                lines.append(f"{indent}{d.name}: \"{val}\"")

    # 写入文件
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))


    