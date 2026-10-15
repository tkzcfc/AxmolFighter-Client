# Shared helper: resolve the default axmol target platform for the current OS.
function Get-DefaultAxPlatform {
    if ($IsWindows -or $env:OS -eq 'Windows_NT') { return 'win32' }
    if ($IsMacOS) { return 'osx' }
    if ($IsLinux) { return 'linux' }
    throw 'Unsupported host OS; pass -Platform explicitly.'
}
