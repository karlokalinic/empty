param(
    [string]$BuildDir = "build",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[submarine_noir] Removing existing build directory: $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

if ((Test-Path "$BuildDir/CMakeCache.txt") -and -not $Clean) {
    $cache = Get-Content "$BuildDir/CMakeCache.txt" -Raw
    $cwd = (Get-Location).Path.Replace('\\', '/')
    if (($cache -match "CMAKE_HOME_DIRECTORY:INTERNAL=(.+)") -and ($Matches[1] -ne $cwd)) {
        Write-Host "[submarine_noir] Detected stale CMake cache from different source path. Recreating build dir..."
        Remove-Item -Recurse -Force $BuildDir
    }
}

cmake -S . -B $BuildDir
cmake --build $BuildDir -j

$exePath = Join-Path $BuildDir "submarine_noir.exe"
if (Test-Path $exePath) {
    Write-Host "[submarine_noir] Running $exePath"
    & $exePath
} else {
    Write-Host "[submarine_noir] Executable not found at $exePath"
    exit 1
}
