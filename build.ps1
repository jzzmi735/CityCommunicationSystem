<#
  城市通信网络设计系统 —— 一键构建与打包脚本

  用法（在项目根目录）：
      .\build.ps1              构建并打包到 dist\
      .\build.ps1 -SkipDist    只构建，不打包

  打包产物 dist\ 可直接拷贝到其它 Windows 电脑运行，无需安装 Qt。

  对应《共同开发规则和接口约定》第 29 节的发布整理要求。

  【重要】本文件必须保存为「UTF-8 带 BOM」。
  Windows PowerShell 5.1 读取无 BOM 的 .ps1 时按系统 ANSI 代码页解析，
  文件里的中文注释会被解码成乱码，其字节还会吞掉紧随其后的代码行——
  症状是脚本「静默跳过」若干语句且不报错，极难排查。
  若用编辑器另存后脚本行为异常，先检查 BOM 是否还在。
#>

param(
    [switch]$SkipDist
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
#  路径配置
#  若 Qt 安装位置不同，修改这里，或通过环境变量 QT_ROOT 指定。
# ---------------------------------------------------------------------------

$QtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt' }
$QtVersion = '6.9.3'
$QtDir   = Join-Path $QtRoot "$QtVersion\mingw_64"
$MingwDir = Join-Path $QtRoot "Tools\mingw1310_64\bin"

if (-not (Test-Path $QtDir)) {
    Write-Host "ERROR: Qt not found at $QtDir" -ForegroundColor Red
    Write-Host "       Set QT_ROOT or edit `$QtDir in this script." -ForegroundColor Red
    exit 1
}

# 必须使用 Qt 自带的 MinGW：Qt 预编译版是 UCRT 运行时，
# 用系统 PATH 中的其它 MinGW（如 Strawberry 的 MSVCRT 版）链接会报
# "undefined reference to __imp___argc"，且错误信息不会提示运行时库不一致。
$env:PATH = "$MingwDir;$QtDir\bin;$env:PATH"

Write-Host "=== CityCommunicationSystem build ===" -ForegroundColor Cyan
Write-Host "Qt      : $QtDir"
Write-Host "MinGW   : $MingwDir"
Write-Host ""

# ---------------------------------------------------------------------------
#  一、构建
# ---------------------------------------------------------------------------

Write-Host "[1/3] Configuring..." -ForegroundColor Yellow
cmake -S . -B build -G "Ninja" `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="$QtDir" `
      -DCMAKE_CXX_COMPILER="$MingwDir\g++.exe" | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: cmake configure failed" -ForegroundColor Red; exit 1 }

Write-Host "[2/3] Building..." -ForegroundColor Yellow
cmake --build build
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: build failed" -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------------------
#  二、运行测试
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "Running tests..." -ForegroundColor Yellow
Push-Location build
ctest --output-on-failure
$testResult = $LASTEXITCODE
Pop-Location
if ($testResult -ne 0) {
    Write-Host "ERROR: tests failed" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
#  三、打包
# ---------------------------------------------------------------------------

if ($SkipDist) {
    Write-Host ""
    Write-Host "Done. (dist skipped)" -ForegroundColor Green
    exit 0
}

Write-Host ""
Write-Host "[3/3] Packaging to dist\ ..." -ForegroundColor Yellow

$DistDir = 'dist'
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item 'build\bin\CityCommunicationSystem.exe' $DistDir
Copy-Item -Recurse 'data' $DistDir
Copy-Item 'README.md' $DistDir

# 部署 Qt 运行时依赖（DLL 与平台插件）。
# 不加 --no-translations：Qt 自带的中文翻译会让标准对话框显示中文。
#
# 注意：不要对原生命令使用 2>$null。PowerShell 5.1 会把重定向的 stderr
# 包装成 NativeCommandError，在 $ErrorActionPreference = 'Stop' 下会直接
# 终止脚本——而且因为输出被丢弃，失败时毫无提示。这里让它正常输出。
& "$QtDir\bin\windeployqt.exe" --release "$DistDir\CityCommunicationSystem.exe"
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: windeployqt failed" -ForegroundColor Red; exit 1 }

# 记录来源版本，便于日后追溯这份可执行文件对应哪次提交。
# 同样不加 2>$null；若当前不在 git 仓库中，git 会自行输出错误但不影响打包。
$commit = (git rev-parse --short HEAD)
$version = @()
$version += "CityCommunicationSystem release package"
$version += "Commit  : $commit"
$version += "Built   : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$version += "Qt      : $QtVersion (mingw_64)"
$version += ""
$version += "Run: double-click CityCommunicationSystem.exe"
$version | Set-Content -Encoding utf8 "$DistDir\VERSION.txt"

Write-Host ""
Write-Host "Package ready: $DistDir\" -ForegroundColor Green
Write-Host "Contents:"
Get-ChildItem $DistDir | ForEach-Object {
    Write-Host ("  {0,-34} {1}" -f $_.Name, $(if ($_.PSIsContainer) { '<dir>' } else { "$([math]::Round($_.Length/1KB,1)) KB" }))
}
Write-Host ""
Write-Host "Copy the dist\ folder to any 64-bit Windows PC and run the exe." -ForegroundColor Cyan
