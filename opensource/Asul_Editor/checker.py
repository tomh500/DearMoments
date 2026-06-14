# checker.py
SKIP_CONFLICT = {'key', 'none'}

def check_conflicts(directives):
    binds = [(d.entry.get().strip(), d.command)
             for d in directives if d.kind == 'key']
    dup = {}
    for k, c in binds:
        dup.setdefault(k, []).append(c)
    return {
        k: v for k, v in dup.items()
        if len(v) > 1 and k.lower() not in SKIP_CONFLICT
    }
