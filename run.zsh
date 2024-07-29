#!/usr/bin/env zsh

executable="cmake-build-debug-g-13-windows/interpreter.exe"
input_dir="testcases"
log_dir="test"
mkdir -p "$log_dir"

if [ -n "$1" ]; then
  name=$1
  input_file="$input_dir/$name.data"

  "$executable" < "$input_file" > "$log_dir/$name.out" 2> "$log_dir/$name.err"
  ret_val=$?

  if [ $ret_val -eq 0 ]; then
    echo "Execution of $name: Success (Return value: 0)"
    echo "Error output (saved to $log_dir/$name.err):"
    tail "$log_dir/$name.err"
    echo "Output (saved to $log_dir/$name.out):"
    cat "$log_dir/$name.out"
  else
    echo "Execution of $name: Failure (Return value: $ret_val)"
    echo "Error output (saved to $log_dir/$name.err):"
    tail "$log_dir/$name.err"
fi

fi
