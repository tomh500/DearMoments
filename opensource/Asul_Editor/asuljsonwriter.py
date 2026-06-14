# asuljsonwriter.py
import os
import shutil
import json
import time

def get_directive_value(d):
    """
    获取指令值，兼容新版/旧版。
    只读，不改变 d.var，保证滑块可用
    """
    if hasattr(d, 'var') and callable(getattr(d.var, 'get', None)):
        return d.var.get()
    elif hasattr(d, 'default'):
        return d.default
    elif hasattr(d, 'entry') and callable(getattr(d.entry, 'get', None)):
        return d.entry.get()
    else:
        return None

def write_json(json_path, directives):
    data = {}
    comments = {}
    for d in directives:
        if getattr(d, 'kind', None) == 'text':
            content = getattr(d, 'content', '')
            detail = getattr(d, 'detail', '')
            comments[content] = detail
    if comments:
        data["__comments"] = comments

    for d in directives:
        kind = getattr(d, 'kind', None)
        if kind not in ('bool', 'int', 'float'):
            continue
        path = getattr(d, 'path', []) or []
        ctx = data
        for seg in path:
            ctx = ctx.setdefault(seg, {})

        value = get_directive_value(d)
        if value is None: continue # 跳过空值

        if kind == 'bool':
            # 兼容处理：如果是字符串 "True"/"False" 也要转成布尔
            if isinstance(value, str):
                ctx[d.name] = value.lower() == 'true'
            else:
                ctx[d.name] = bool(value)
        elif kind == 'int':
            ctx[d.name] = int(float(value)) # 先转 float 再转 int，防止 "1.0" 报错
        elif kind == 'float':
            ctx[d.name] = float(value)

    folder = os.path.dirname(json_path)
    backup_dir = os.path.join(folder, 'backup')
    os.makedirs(backup_dir, exist_ok=True)
    if os.path.isfile(json_path):
        base = os.path.basename(json_path)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        shutil.copy2(json_path, os.path.join(backup_dir, f"{base}.{timestamp}.bak"))

    os.makedirs(folder, exist_ok=True)
    with open(json_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write('\n')


def save_asuljson(path, directives):
    lines = []
    for d in directives:
        kind = getattr(d, 'kind', None)
        if kind == 'text':
            lines.append(f'text "{getattr(d, "content","")}" "{getattr(d,"detail","")}"')
        elif kind == 'bool':
            location = '/' + '/'.join(getattr(d, 'path', []) or []) if getattr(d, 'path', None) else "/"
            default = "true" if get_directive_value(d) else "false"
            lines.append(f'bool "{d.name}" "{default}" "{location}"')
        elif kind == 'int':
            location = '/' + '/'.join(getattr(d, 'path', []) or []) if getattr(d, 'path', None) else "/"
            minimum = getattr(d, 'minimum', 0)
            maximum = getattr(d, 'maximum', 100)
            default = get_directive_value(d) or getattr(d, 'default', 0)
            lines.append(f'int "{d.name}" "{minimum},{maximum},{default}" "{location}"')
        elif kind == 'float':
            location = '/' + '/'.join(getattr(d, 'path', []) or []) if getattr(d, 'path', None) else "/"
            minimum = getattr(d, 'minimum', 0.0)
            maximum = getattr(d, 'maximum', 1.0)
            default = get_directive_value(d) or getattr(d, 'default', 0.0)
            lines.append(f'float "{d.name}" "{minimum},{maximum},{default}" "{location}"')
        else:
            lines.append(getattr(d, 'content', ''))

    folder = os.path.dirname(path)
    backup_dir = os.path.join(folder, 'backup')
    os.makedirs(backup_dir, exist_ok=True)
    if os.path.isfile(path):
        base = os.path.basename(path)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        shutil.copy2(path, os.path.join(backup_dir, f"{base}.{timestamp}.bak"))

    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write("\n".join(lines))
        f.write('\n')
