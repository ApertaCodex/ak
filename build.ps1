# Cross-platform build script for AK (API Key Manager) - Windows PowerShell version
# Works on Windows with PowerShell 5.0+

param(
    [string]$BuildType = "Release",
    [string[]]$Targets = @("build"),
    [switch]$Debug,
    [switch]$Coverage,
    [switch]$Help
)

# Colors for output (if supported)
$script:SupportsColor = $Host.UI.RawUI.ForegroundColor -ne $null

function Write-Header {
    Write-Host "========================================" -ForegroundColor Blue
    Write-Host "  AK (API Key Manager) Build Script" -ForegroundColor Blue
    Write-Host "  Platform: Windows" -ForegroundColor Blue
    Write-Host "========================================" -ForegroundColor Blue
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    Write-Host "🔧 $Message" -ForegroundColor Yellow
}

function Write-Success {
    param([string]$Message)
    Write-Host "✅ $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "❌ $Message" -ForegroundColor Red
    exit 1
}

function Test-Requirements {
    Write-Step "Checking build requirements..."
    
    # Check for CMake
    try {
        $cmakeVersion = & cmake --version 2>&1 | Select-String -Pattern "cmake version" | ForEach-Object { $_.Line -replace ".*cmake version (\d+\.\d+\.\d+).*", '$1' }
        $versionParts = $cmakeVersion.Split('.')
        $major = [int]$versionParts[0]
        $minor = [int]$versionParts[1]
        
        if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 16)) {
            Write-Error "CMake 3.16 or higher is required. Found version: $cmakeVersion"
        }
    }
    catch {
        Write-Error "CMake is required but not installed. Please install CMake 3.16 or higher."
    }
    
    # Check for C++ compiler
    $compiler = "Unknown"
    try {
        & cl 2>$null
        $compiler = "MSVC"
    }
    catch {
        try {
            & g++ --version 2>$null
            $compiler = "MinGW GCC"
        }
        catch {
            try {
                & clang++ --version 2>$null
                $compiler = "Clang"
            }
            catch {
                Write-Error "No C++ compiler found. Please install Visual Studio, MinGW, or Clang."
            }
        }
    }
    
    Write-Success "Requirements check passed. Using $compiler compiler."
}

function Invoke-ConfigureBuild {
    param([string]$Type)
    
    Write-Step "Configuring build system..."
    
    # Create build directory
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
    New-Item -ItemType Directory -Path "build" | Out-Null
    Set-Location "build"
    
    # Configure with CMake
    $cmakeArgs = @("..")
    
    # Try to detect Visual Studio
    try {
        & cl 2>$null
        $cmakeArgs += @("-G", "Visual Studio 16 2019", "-A", "x64")
    }
    catch {
        # Fall back to default generator
    }
    
    $cmakeArgs += @("-DCMAKE_BUILD_TYPE=$Type")
    
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Set-Location ..
        Write-Error "CMake configuration failed"
    }
    
    Set-Location ..
    Write-Success "Build system configured successfully"
}

function Invoke-BuildProject {
    Write-Step "Building project..."
    
    Set-Location "build"
    
    # Build with parallel jobs
    $cores = $env:NUMBER_OF_PROCESSORS
    & cmake --build . --parallel $cores
    if ($LASTEXITCODE -ne 0) {
        Set-Location ..
        Write-Error "Build failed"
    }
    
    Set-Location ..
    Write-Success "Project built successfully"
}

function Invoke-RunTests {
    Write-Step "Running tests..."
    
    Set-Location "build"
    
    # Find and run the test executable
    $testExe = $null
    if (Test-Path "Release\ak_tests.exe") {
        $testExe = "Release\ak_tests.exe"
    }
    elseif (Test-Path "Debug\ak_tests.exe") {
        $testExe = "Debug\ak_tests.exe"
    }
    elseif (Test-Path "ak_tests.exe") {
        $testExe = "ak_tests.exe"
    }
    else {
        Set-Location ..
        Write-Error "Test executable not found"
    }
    
    & $testExe
    if ($LASTEXITCODE -ne 0) {
        Set-Location ..
        Write-Error "Tests failed"
    }
    
    Set-Location ..
    Write-Success "All tests passed"
}

function Invoke-GenerateCoverage {
    Write-Host "⚠️  Coverage reporting is not supported on Windows" -ForegroundColor Yellow
}

function Invoke-InstallProject {
    Write-Step "Installing project..."
    
    Set-Location "build"
    
    & cmake --build . --target INSTALL
    if ($LASTEXITCODE -ne 0) {
        Set-Location ..
        Write-Error "Installation failed"
    }
    
    Set-Location ..
    Write-Success "Project installed successfully"
}

function Show-Help {
    Write-Host "Usage: .\build.ps1 [OPTIONS] [TARGETS]"
    Write-Host ""
    Write-Host "OPTIONS:"
    Write-Host "  -Debug          Build in debug mode"
    Write-Host "  -Coverage       Build with coverage instrumentation (not supported on Windows)"
    Write-Host "  -Help           Show this help message"
    Write-Host ""
    Write-Host "TARGETS:"
    Write-Host "  configure       Configure build system only"
    Write-Host "  build           Build the project (default)"
    Write-Host "  test            Run tests"
    Write-Host "  coverage        Generate coverage report (not supported on Windows)"
    Write-Host "  install         Install the project"
    Write-Host "  clean           Clean build directory"
    Write-Host "  all             Configure, build, and test"
    Write-Host ""
    Write-Host "EXAMPLES:"
    Write-Host "  .\build.ps1                     # Build in release mode"
    Write-Host "  .\build.ps1 -Debug              # Build in debug mode"
    Write-Host "  .\build.ps1 -Targets test       # Run tests only"
    Write-Host "  .\build.ps1 -Targets all        # Full build and test cycle"
}

# Main script
if ($Help) {
    Show-Help
    exit 0
}

if ($Debug) {
    $BuildType = "Debug"
}
elseif ($Coverage) {
    $BuildType = "Coverage"
    Write-Host "⚠️  Coverage mode is not fully supported on Windows" -ForegroundColor Yellow
}

# Handle clean target
if ($Targets -contains "clean") {
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Success "Build directory cleaned"
    }
    else {
        Write-Host "Build directory doesn't exist"
    }
    exit 0
}

# Default to build if no targets specified
if ($Targets.Count -eq 0) {
    $Targets = @("build")
}

Write-Header
Test-Requirements

# Execute targets
foreach ($target in $Targets) {
    switch ($target) {
        "configure" {
            Invoke-ConfigureBuild $BuildType
        }
        "build" {
            Invoke-ConfigureBuild $BuildType
            Invoke-BuildProject
        }
        "test" {
            if (-not (Test-Path "build")) {
                Invoke-ConfigureBuild $BuildType
                Invoke-BuildProject
            }
            Invoke-RunTests
        }
        "coverage" {
            Invoke-GenerateCoverage
        }
        "install" {
            if (-not (Test-Path "build")) {
                Invoke-ConfigureBuild $BuildType
                Invoke-BuildProject
            }
            Invoke-InstallProject
        }
        "all" {
            Invoke-ConfigureBuild $BuildType
            Invoke-BuildProject
            Invoke-RunTests
        }
        default {
            Write-Error "Unknown target: $target"
        }
    }
}

Write-Host ""
Write-Success "Build script completed successfully!"

# Show binary location
$binaryPath = $null
if (Test-Path "build\Release\ak.exe") {
    $binaryPath = "build\Release\ak.exe"
}
elseif (Test-Path "build\Debug\ak.exe") {
    $binaryPath = "build\Debug\ak.exe"
}
elseif (Test-Path "build\ak.exe") {
    $binaryPath = "build\ak.exe"
}

if ($binaryPath) {
    Write-Host ""
    Write-Host "Binary location:" -ForegroundColor Blue
    Write-Host "  $binaryPath"
}