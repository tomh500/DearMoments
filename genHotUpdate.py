import os

# ================= 配置区 =================
# 1. 你本地项目的根目录路径 ('.' 代表当前目录)
PROJECT_ROOT = '.' 

# 2. 远程仓库的基础 URL
BASE_URL = "https://raw.githubusercontent.com/tomh500/DearMoments/refs/heads/HotUpdate"

# 3. 输出文件名
OUTPUT_FILE = 'HotUpdate.txt'

# 4. 忽略名单 (不希望上传到云端更新的文件或文件夹)
IGNORE_LIST = {
    '.git', '.gitignore', '.gitattributes', 
    'HotUpdate.txt', '.vs', 'out', 'build',
    'library/resource/cache'
}

# 5. 忽略的后缀名
IGNORE_EXTENSIONS = {'.bak', '.tmp', '.old', '.pdb', '.obj', '.exe', '.dll'}
# ==========================================

def generate_hot_update():
    lines = []
    
    for root, dirs, files in os.walk(PROJECT_ROOT):
        # 排除忽略目录
        dirs[:] = [d for d in dirs if d not in IGNORE_LIST]
        
        for file in files:
            # 排除忽略的后缀名
            if any(file.endswith(ext) for ext in IGNORE_EXTENSIONS):
                continue
            if file in IGNORE_LIST:
                continue

            # 获取相对路径 (例如: scripts/legacy/SQ/4items/Nogui.cfg)
            full_path = os.path.join(root, file)
            relative_path = os.path.relpath(full_path, PROJECT_ROOT)
            
            # C++ 函数里会把 \ 换成 /，我们这里统一处理成正斜杠
            safe_path = relative_path.replace('\\', '/')
            
            # 构造 C++ 需要的格式
            # "路径"
            # - URL
            lines.append(f'"{safe_path}"')
            lines.append(f'- {BASE_URL}/{safe_path}')
            lines.append('') # 换行美化

    # 写入文件
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"成功生成 {OUTPUT_FILE}，共处理 {len(lines)//3} 个文件。")

if __name__ == "__main__":
    generate_hot_update()