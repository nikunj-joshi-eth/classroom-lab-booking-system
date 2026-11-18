# Run the exact annotated C program with the SAME storage as the web dashboard.
# Run from any directory: .\run_console.ps1
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot 'backend')
try {
    gcc -std=c99 -Wall -Wextra -Wconversion -pedantic main.c -o booking.exe
    if ($LASTEXITCODE -ne 0) { throw 'C compilation failed.' }
    & .\booking.exe
} finally {
    Pop-Location
}
