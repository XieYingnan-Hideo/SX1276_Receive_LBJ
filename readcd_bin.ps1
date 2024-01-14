$filepath = "./COREDUMP*.bin"
"Executing script..."
if (Test-Path $filepath) {
    if ((Get-ChildItem -Filter $filepath -Name | Measure-Object ).Count -eq 1) {
        $filename = Get-ChildItem -Filter $filepath -Name
    } else {
        "Multiple Files Found."
        Get-ChildItem -Filter $filepath
        Return
    }
} else {
    "$filepath not found"
    Return
}

"Building ESP-IDF Environment..."
E:\Espressif/Initialize-Idf.ps1 -IdfId esp-idf-13982a4ae7695f7e87ad379f601c2330

"Executing espcoredump.py"
python "E:\Espressif\frameworks\esp-idf-v5.1.1\components\espcoredump\espcoredump.py" --chip esp32 info_corefile -c $filename -t raw .\.pio\build\esp32dev\firmware.elf
