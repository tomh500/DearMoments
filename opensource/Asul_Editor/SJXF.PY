# fix_indent.py
import sys
import re

def fix_indentation(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    fixed_lines = []
    indent_stack = [0]
    current_indent = 0

    for line in lines:
        # 原始空白去除并统一制表符为空格
        raw = line.replace('\t', '    ')
        stripped = raw.lstrip()
        if not stripped:
            fixed_lines.append('\n')
            continue

        # 简单判断缩进级别：每层 4 个空格
        leading_spaces = len(raw) - len(stripped)
        indent_level = leading_spaces // 4
        corrected_indent = '    ' * indent_level
        fixed_lines.append(corrected_indent + stripped)

    # 写入新文件（或覆盖）
    new_path = file_path.replace('.py', '_fixed.py')
    with open(new_path, 'w', encoding='utf-8') as f:
        f.writelines(fixed_lines)

    print(f'✅ 缩进修复完成，已保存为: {new_path}')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("用法：python fix_indent.py yourfile.py")
        sys.exit(1)

    fix_indentation(sys.argv[1])
