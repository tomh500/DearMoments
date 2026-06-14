# 1. 定义检查函数
function Check-Env {
    Write-Host ">>> 正在检查构建环境..." -ForegroundColor Cyan

    # --- 检查 Python ---
    if (-not (Get-Command "python" -ErrorAction SilentlyContinue)) {
        Write-Warning "未找到 Python！"
        $choice = Read-Host "是否现在去下载 Python? (y/n)"
        if ($choice -eq 'y') { Start-Process "https://www.python.org/downloads/" }
        exit 1
    }

    # --- 检查 Python 库 (这里以 requests 为例) ---
    python -c "import requests" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "正在安装依赖库..."
        python -m pip install requests
    }

    # --- 检查 Visual Studio (使用 vswhere) ---
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Warning "未安装 Visual Studio 或 vswhere!"
        $choice = Read-Host "请安装 VS2022 社区版及 C++ 开发组件，是否跳转下载? (y/n)"
        if ($choice -eq 'y') { Start-Process "https://visualstudio.microsoft.com/vs/community/" }
        exit 1
    }

    # 检查是否有 C++ 工作负载
    $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
    if (-not $vsPath) {
        Write-Warning "VS 已安装，但缺少 C++ 桌面开发组件！"
        Write-Host "请在 VS Installer 中勾选 '使用 C++ 的桌面开发'"
        exit 1
    }
    
    Write-Host "环境检查通过！" -ForegroundColor Green
}

# 2. 执行检查
Check-Env

# 3. 原有逻辑
cd opensource
python .\TestBuild.py