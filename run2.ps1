$executable = "cmake-build-debug-g-13-windows/interpreter.exe"
$inputDir = "testcases"
$logDir = "test"
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

if ($args.Count -gt 0) {
    $name = $args[0]
    $inputFile = Join-Path $inputDir "$name.data"
    $outputFile = Join-Path $logDir "$name.out"
    $errorFile = Join-Path $logDir "$name.err"

    # Execute the command
    & $executable < $inputFile *> $outputFile 2> $errorFile
    $retVal = $LASTEXITCODE

    if ($retVal -eq 0) {
        Write-Host "Execution of ${name}: Success (Return value: 0)"
        Write-Host "Error output (saved to $errorFile):"
        Get-Content $errorFile -Tail 10
        Write-Host "Output (saved to $outputFile):"
        Get-Content $outputFile
    } else {
        Write-Host "Execution of ${name}: Failure (Return value: $retVal)"
    }
}
