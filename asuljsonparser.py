# asuljsonparser.py
import re
from asulparser import Directive  # 重用主项目中的 Directive 定义

# 匹配每种指令格式（DSL）
PAT_JSON_TEXT  = re.compile(r'^text\s+"([^"]+)"\s+"([^"]+)"\s*$')
PAT_JSON_BOOL  = re.compile(r'^bool\s+"([^"]+)"\s+"(true|false)"\s+"([^"]+)"\s*$')
PAT_JSON_INT   = re.compile(r'^int\s+"([^"]+)"\s+"(-?\d+),(-?\d+),(-?\d+)"\s+"([^"]+)"\s*$')
PAT_JSON_FLOAT = re.compile(r'^float\s+"([^"]+)"\s+"([0-9.]+),([0-9.]+),([0-9.]+)"\s+"([^"]+)"\s*$')

def parse_asuljson(path):
    """
    解析 .asuljson 文件，返回 Directive 列表，并自动绑定注释。
    """
    lines = open(path, encoding='utf-8').read().splitlines()
    ds = []
    comment_map = {}

    for line in lines:
        line = line.strip()
        if not line: continue
        
        if m := PAT_JSON_TEXT.match(line):
            title, detail = m.groups()
            comment_map[title] = detail
            ds.append(Directive('text', content=title, detail=detail))
            continue

        if m := PAT_JSON_BOOL.match(line):
            name, default, location = m.groups()
            default_bool = (default.lower() == 'true')
            path_seg = [seg for seg in location.split('/') if seg]
            d = Directive('bool', name=name, title=name, default=default_bool, path=path_seg)
            d.comment = comment_map.get(name, "")
            ds.append(d)
            continue

        if m := PAT_JSON_INT.match(line):
            name, mn, mx, df, location = m.groups()
            path_seg = [seg for seg in location.split('/') if seg]
            d = Directive('int', name=name, title=name, default=int(df),
                         minimum=int(mn), maximum=int(mx), step=1, path=path_seg)
            d.comment = comment_map.get(name, "")
            ds.append(d)
            continue

        if m := PAT_JSON_FLOAT.match(line):
            name, mn, mx, df, location = m.groups()
            path_seg = [seg for seg in location.split('/') if seg]
            d = Directive('float', name=name, title=name, default=float(df),
                         minimum=float(mn), maximum=float(mx), step=0.01, path=path_seg)
            d.comment = comment_map.get(name, "")
            ds.append(d)
            continue

        ds.append(Directive('raw', content=line))
    return ds