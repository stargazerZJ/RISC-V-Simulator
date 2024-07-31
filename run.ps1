param (
    [string]$name
)

$executable = "cmake-build-debug-g-13-windows\interpreter.exe"
$input_dir = "testcases"
$log_dir = "test"

# Create the log directory if it doesn't exist
if (!(Test-Path -Path $log_dir)) {
    New-Item -ItemType Directory -Path $log_dir | Out-Null
}

if ($name) {
    $input_file = Join-Path -Path $input_dir -ChildPath "$name.data"
    $out_file = Join-Path -Path $log_dir -ChildPath "$name.out"
    $err_file = Join-Path -Path $log_dir -ChildPath "$name.err"

#     $process = Start-Process -FilePath $executable -ArgumentList "@($input_file)" -RedirectStandardOutput $out_file -RedirectStandardError $err_file -PassThru
    $process = Start-Process -FilePath $executable -RedirectStandardInput $input_file -RedirectStandardOutput $out_file -RedirectStandardError $err_file -PassThru

    $process.WaitForExit()
    $ret_val = $process.ExitCode

#     if ($ret_val -eq 0) {
#         Write-Output "Execution of ${name}: Success (Return value: 0)"
        Write-Output "Execution of ${name} finished"
        Write-Output "Error output (saved to $err_file):"
        Get-Content $err_file | Select-Object -Last 10
        Write-Output "Output (saved to $out_file):"
        Get-Content $out_file
#     } else {
#         Write-Output "Execution of ${name}: Failure (Return value: $ret_val)"
#     }
}
