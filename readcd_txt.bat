@echo off
(
    echo # Generated ps1 file.
    echo $filepath = "./coredump.txt"
    echo "Executing script..."
    echo.
    echo if ^(Test-Path $filepath^) ^{
    echo     $hex = Get-Content -Path $filepath -Raw
    echo ^} else ^{
    echo     "$filepath not found"
    echo     Remove-Item "./runtime_script.ps1"
    echo     Return
    echo ^}
    echo ^[byte^[^]^]$bytes = ^($hex -split '^(.^{2^}^)' -ne '' -replace '^^', '0X'^)
    echo [System.IO.File]::WriteAllBytes^("./coredump.bin", $bytes^)
    echo "Building ESP-IDF Environment..."
    echo E:\Espressif/Initialize-Idf.ps1 -IdfId esp-idf-13982a4ae7695f7e87ad379f601c2330
    echo "Executing espcoredump.py"
    echo python "E:\Espressif\frameworks\esp-idf-v5.1.1\components\espcoredump\espcoredump.py" --chip esp32 info_corefile -c .\coredump.bin -t raw .\.pio\build\esp32dev\firmware.elf
    echo Remove-Item "./coredump.bin"
    echo Remove-Item "./runtime_script.ps1"
    echo # End of file.
) > "./runtime_script.ps1"

C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -NoExit -File "./runtime_script.ps1"