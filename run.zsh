#!/usr/bin/env zsh

executable="cmake-build-debug-g-13-windows/code.exe"
input_dir="testcases"
log_dir="test"
mkdir -p "$log_dir"

run_test () {
  local name=$1
  local input_file="$input_dir/$name.data"

  echo "Testing: $name"
  "$executable" < "$input_file" > "$log_dir/$name.out" 2> "$log_dir/$name-sim.err"
  local ret_val=$?

  if [ $ret_val -eq 0 ]; then
    echo "Execution of $name: Success (Return value: 0)"
    echo "Error output (saved to $log_dir/$name-sim.err):"
    tail "$log_dir/$name-sim.err"
    echo "Output (saved to $log_dir/$name.out):"
    cat "$log_dir/$name.out"
  else
    echo "Execution of $name: Failure (Return value: $ret_val)"
    echo "Error output (saved to $log_dir/$name-sim.err):"
    tail "$log_dir/$name-sim.err"
  fi
}

if [ -n "$1" ]; then
  name=$1
  run_test "$name"
else
  for input_file in "$input_dir"/*.data; do
    [[ -e "$input_file" ]] || break # handle case when there are no .data files
    name=$(basename "$input_file" .data)
    run_test "$name"
  done
fi
