#!/bin/bash
# Test suite for minishell

echo "Running tests for minishell..."

if [ ! -f ./minishell ]; then
    echo "minishell binary not found. Run 'make' first."
    exit 1
fi

FAIL=0
PASS=0

run_test() {
    local test_name="$1"
    local cmd="$2"
    local expected_out="$3"

    # Use standard shell to get expected output if not provided, for simplicity we provide it
    echo -n "$cmd" | ./minishell > test_out.txt 2> test_err.txt
    
    # Check output
    if grep -q "$expected_out" test_out.txt; then
        echo "[PASS] $test_name"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $test_name (Expected '$expected_out', got: $(cat test_out.txt))"
        FAIL=$((FAIL + 1))
    fi
}

echo "hello world" > dummy.txt

run_test "Basic execution" "echo hello" "hello"
run_test "Pipe execution" "echo hello | wc -c" "6"
run_test "Input redirection" "cat < dummy.txt" "hello world"

echo "test output" | ./minishell > /dev/null 2>&1
# test output redirection
echo "echo success > test_redirect.txt" | ./minishell
if grep -q "success" test_redirect.txt; then
    echo "[PASS] Output redirection"
    PASS=$((PASS + 1))
else
    echo "[FAIL] Output redirection"
    FAIL=$((FAIL + 1))
fi

rm -f dummy.txt test_out.txt test_err.txt test_redirect.txt

echo "======================"
echo "Tests Passed: $PASS"
echo "Tests Failed: $FAIL"
echo "======================"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
