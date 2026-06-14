import subprocess
import shutil
import os
from pathlib import Path

projects = [
    {"name": "CS2-GSI",      "type": "sln"},
    {"name": "DMListener",   "type": "sln"},
    {"name": "MDReader",     "type": "sln"},
    {"name": "CS2MouseHook", "type": "sln"},
    {"name": "Asul_Editor",  "type": "python"},
    {"name": "LinkListener", "type": "cpp"}
]

def get_vcvars_path():
    # 尝试使用 vswhere 自动寻找 VS 安装路径
    vswhere_path = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    
    if vswhere_path.exists():
        # 获取最新版本 VS 的安装路径
        cmd = [str(vswhere_path), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip():
            vs_path = Path(result.stdout.strip())
            vcvars = vs_path / "VC/Auxiliary/Build/vcvars64.bat"
            if vcvars.exists():
                return str(vcvars)
    
    return r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

def build_all():


    root_dir = Path.cwd()
    results = {"success": [], "failed": []}
    
    # VS 环境
    vs_vcvars = get_vcvars_path()
    print(f"正在使用 VC 环境: {vs_vcvars}")
    base_cmd = f'call "{vs_vcvars}" && '

    for p in projects:
        print(f"\n>>> 正在构建: {p['name']} ...")
        project_dir = root_dir / p["name"]
        
        try:
            if p["type"] == "sln":
                sln_file = list(project_dir.glob("*.sln"))[0]
                subprocess.run(f'{base_cmd} msbuild "{sln_file}" /p:Configuration=Release /p:Platform=x64 /t:Rebuild', shell=True, check=True)
            elif p["type"] == "cpp":
                cpp_file = (root_dir / f"{p['name']}.cpp").resolve()
                subprocess.run(f'{base_cmd} cl /EHsc /O2 /std:c++17 /Fe:"{p["name"]}.exe" "{cpp_file}"', shell=True, check=True)
            elif p["type"] == "python":
                os.chdir(project_dir)
                subprocess.run("build.bat", shell=True, check=True)
                os.chdir(root_dir)

            elif p["type"] == "bat": # 新增逻辑
                os.chdir(project_dir)
                # 因为你的 build.bat 里面使用了 cl，所以必须确保 VCVARS 环境已经加载
                # 这里直接拼接 base_cmd 确保环境正确
                subprocess.run(f'{base_cmd} build.bat', shell=True, check=True)
                os.chdir(root_dir)
            results["success"].append(p["name"])
        except Exception as e:
            os.chdir(root_dir)
            results["failed"].append(p["name"])
            print(f"  [失败] {p['name']}: {e}")

    print("\n构建完成。")

def archive_results():
    root_dir = Path.cwd()
    target_dir = (root_dir.parent / "library" / "execute").resolve()
    target_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n>>> 正在将所有产物平铺归档至: {target_dir} ...")
    
    exts = {".exe", ".dll", ".ico"}
    
    # 扫描
    for item in root_dir.rglob("*"):
        # 优化：跳过 git、.vs、build 等干扰目录，只看特定的 Release/Debug 目录或根目录
        if any(part in [".git", ".vs", "obj", "temp"] for part in item.parts):
            continue
            
        if item.is_file() and item.suffix in exts:
            # 排除掉源文件
            if item.suffix == ".cpp" or item.name == "TestBuild.py":
                continue
            
            # 如果你要收集的是编译产物，确保它是 Release 下的
            # 如果它是子目录下的产物，直接 copy
            shutil.copy2(item, target_dir / item.name)
            print(f"  [已归档] {item.name}")

if __name__ == "__main__":
    build_all()      # 1. 先进行所有项目的编译
    archive_results() # 2. 编译后统一收割所有产物