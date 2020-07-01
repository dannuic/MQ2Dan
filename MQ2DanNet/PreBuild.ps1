param([string]$VCPkgRoot);
Set-Location $VCPkgRoot
if (!(Test-Path ".\vcpkg.exe")) {
    & ".\bootstrap-vcpkg.bat"
}
& ".\vcpkg.exe" install czmq[draft]:x86-windows-static zyre:x86-windows-static
& ".\vcpkg.exe" integrate install