# asulwriter.py
import os, shutil
import time

SKIP_CONFLICT = {'key','none'}

def get_directive_value(d):
    """
    获取指令值，兼容新版/旧版。
    只读，不改变 d.var，保证滑块可用。
    """
    if hasattr(d, 'var') and callable(getattr(d.var, 'get', None)):
        return d.var.get()
    elif hasattr(d, 'default'):
        return d.default
    elif hasattr(d, 'entry') and callable(getattr(d.entry, 'get', None)):
        return d.entry.get()
    else:
        return None


def write_cfg(path, directives):
    out = []
    for d in directives:
        kind = getattr(d, 'kind', None)
        if kind == 'text':
            out.append(getattr(d, 'content', ''))
        elif kind == 'func':
            val = get_directive_value(d)
            out.append(str(val) if val is not None else '')
        elif kind == 'key':
            kv = get_directive_value(d)
            kv = kv.strip() if kv else "key"
            cmd = getattr(d, 'command', '')
            out.append(f"bind {kv} {cmd}")
        elif kind == 'line':
            val = get_directive_value(d)
            val = val.strip() if val else ''
            template = getattr(d, 'template', '%1')
            if val:
                out.append(template.replace("%1", val))
        else:
            out.append(getattr(d, 'content', ''))

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write("\n".join(out))
        f.write('\n')

def save_asul(path, directives):
    new = []
    for d in directives:
        kind = getattr(d, 'kind', None)
        if kind == 'text':
            new.append(f'text "{getattr(d, "content","")}"')
        elif kind == 'func':
            val = get_directive_value(d)
            cnt = len(getattr(d, 'options', []))
            new.append(f'func "{getattr(d,"title","")}" "{getattr(d,"detail","")}" "{val}" {cnt}')
            for k, desc in getattr(d, 'options', []):
                new.append(f'    "{k}" "{desc}"')
        elif kind == 'key':
            kv = get_directive_value(d)
            kv = kv.strip() if kv else "key"
            new.append(f'key "{kv}" "{getattr(d,"command","")}" "{getattr(d,"title","")}" "{getattr(d,"detail","")}"')
        elif kind == 'line':
            val = get_directive_value(d)
            val = val.strip() if val else ''
            new.append(f'line "{getattr(d,"title","")}" "{getattr(d,"detail","")}" "{getattr(d,"template","")}" "{val}"')
        else:
            new.append(getattr(d, 'content', ''))

    bak = os.path.join(os.path.dirname(path), "backup")
    os.makedirs(bak, exist_ok=True)
    if os.path.isfile(path):
        base = os.path.basename(path)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        shutil.copy2(path, os.path.join(bak, f"{base}.{timestamp}.bak"))

    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write("\n".join(new))
        f.write('\n')
