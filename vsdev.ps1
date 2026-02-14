$vsDevCmd = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

if (!(Test-Path $vsDevCmd)) {
    Write-Error "VsDevCmd.bat not found"
    return
}

$envDump = cmd /c "`"$vsDevCmd`" && set"

foreach ($line in $envDump) {
    if ($line -match "^(.*?)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

Write-Host "Visual Studio build environment loaded ✔"
