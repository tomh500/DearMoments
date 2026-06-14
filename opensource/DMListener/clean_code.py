import os

def clean_file(file_path):
    try:
        # 1. 以二进制模式读取，避免 Python 自动处理换行符或 BOM
        with open(file_path, 'rb') as f:
            content = f.read()

        # 2. 如果有 UTF-8 BOM (\xef\xbb\xbf)，直接切掉
        if content.startswith(b'\xef\xbb\xbf'):
            content = content[3:]

        # 3. 解码为字符串，替换掉不可见的非法字符
        # 'ignore' 会直接丢弃无法解码的字节，'replace' 会变成问号
        text = content.decode('utf-8', errors='ignore')

        # 4. 过滤掉常见的“污染源”
        # \u200b: 零宽空格 (Zero Width Space)
        # \u200c, \u200d: 零宽连字符等
        # \ufeff: 字节顺序标记 (BOM) 在文本中间出现的情况
        bad_chars = ['\u200b', '\u200c', '\u200d', '\ufeff']
        for char in bad_chars:
            text = text.replace(char, '')

        # 5. 确保换行符统一（可选，VS 通常能自动处理 CR/LF）
        # text = text.replace('\r\n', '\n').replace('\n', '\r\n')

        # 6. 写回文件，指定 utf-8 编码且不带 BOM
        with open(file_path, 'w', encoding='utf-8', newline='') as f:
            f.write(text)

        print(f"Successfully cleaned: {file_path}")

    except Exception as e:
        print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    # 在这里填入你的 .cpp 或 .h 文件名
    target_files = ['MusicLegacy.cpp']
    
    for file in target_files:
        if os.path.exists(file):
            clean_file(file)
        else:
            print(f"File not found: {file}")