@echo off
(
    echo # Generated ps1 file.
    echo $filepath = "./COREDUMP*.bin"
    echo "Executing script..."
    echo.
    echo if ^(Test-Path $filepath^) ^{
    echo    if ^(^(Get-ChildItem -Filter $filepath -Name ^| Measure-Object ^).Count -eq 1^) ^{    
    echo        $filename = Get-ChildItem -Filter $filepath -Name
    echo    ^} else ^{
    echo        "Multiple Files Found."
    echo        Get-ChildItem -Filter $filepath
    echo        Return
    echo    ^}
    echo ^} else ^{
    echo     "$filepath not found"
    echo     Remove-Item "./runtime_script2.ps1"
    echo     Return
    echo ^}
    echo "Building ESP-IDF Environment..."
    echo E:\Espressif/Initialize-Idf.ps1 -IdfId esp-idf-13982a4ae7695f7e87ad379f601c2330
    echo "Executing espcoredump.py"
    echo python "E:\Espressif\frameworks\esp-idf-v5.1.1\components\espcoredump\espcoredump.py" --chip esp32 info_corefile -c $filename -t raw .\.pio\build\esp32dev\firmware.elf
    echo Remove-Item "./runtime_script2.ps1"
    echo # End of file.
) > "./runtime_script2.ps1"

C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -NoExit -File "./runtime_script2.ps1"