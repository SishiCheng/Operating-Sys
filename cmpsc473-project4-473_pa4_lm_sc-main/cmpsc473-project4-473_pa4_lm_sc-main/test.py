#!/usr/bin/env python3

import os
import subprocess
import signal
import math
import re
import sys

# Test multiplier
timeout_multiplier = 1 # increase to extend all timeouts
iters_multiplier = 1 # increase to test the code with more iterations

# Timeouts (seconds)
timeout_channel = 3 * timeout_multiplier
timeout_sanitize = 5 * timeout_multiplier
timeout_valgrind = 10 * timeout_multiplier
timeout_cpu_utilization = 40 * timeout_multiplier
timeout_response_time = 40 * timeout_multiplier
timeout_basic_global_declaration = 30 * timeout_multiplier
timeout_too_many_wakeups = 20 * timeout_multiplier
timeout_stress_send_recv = 10 * timeout_multiplier
timeout_non_blocking_receive = 5 * timeout_multiplier
timeout_select_mixed_buffered_unbuffered = 5 * timeout_multiplier
timeout_make = 30 * timeout_multiplier

# Number of iterations to run tests
iters_channel = math.ceil(5000 * iters_multiplier)
iters_sanitize = math.ceil(1000 * iters_multiplier)
iters_valgrind = math.ceil(500 * iters_multiplier)
iters_slow = math.ceil(30 * iters_multiplier)
iters_one = math.ceil(1 * iters_multiplier)

# Test cases
test_cases = {}

def add_test_case_channel(test_name, iters=0, timeout=0):
    test_cases["channel_{}".format(test_name)] = {"args": ["./wordcount", test_name, str(iters_channel if iters == 0 else iters)], "timeout": timeout_channel if timeout == 0 else timeout}

def add_test_case_sanitize(test_name, iters=0, timeout=0):
    test_cases["sanitize_{}".format(test_name)] = {"args": ["./wordcount_sanitize", test_name, str(iters_sanitize if iters == 0 else iters)], "timeout": timeout_sanitize if timeout == 0 else timeout}

def add_test_case_valgrind(test_name, iters=0, timeout=0):
    test_cases["valgrind_{}".format(test_name)] = {"args": ["valgrind", "-v", "--leak-check=full", "--errors-for-leak-kinds=all", "--error-exitcode=2", "./wordcount", test_name, str(iters_valgrind if iters == 0 else iters)], "timeout": timeout_valgrind if timeout == 0 else timeout}

def add_test_cases(test_name, iters=0, timeout=0):
    add_test_case_channel(test_name, iters, timeout)
    add_test_case_sanitize(test_name, iters, timeout)
    add_test_case_valgrind(test_name, iters, timeout)

add_test_cases("test_initialization")

add_test_cases("test_Free")
add_test_cases("test_send_correctness", iters_slow)
add_test_cases("test_receive_correctness", iters_slow)
add_test_case_channel("test_overall_send_receive", iters_one)
add_test_case_sanitize("test_overall_send_receive", iters_one)
add_test_case_valgrind("test_overall_send_receive", iters_one, timeout_valgrind * 5)
add_test_cases("test_cpu_utilization_send", iters_one, timeout_cpu_utilization)
add_test_cases("test_cpu_utilization_receive", iters_one, timeout_cpu_utilization)
add_test_case_channel("test_channel_close_with_send", iters_slow)
add_test_case_sanitize("test_channel_close_with_send", iters_slow)
add_test_case_valgrind("test_channel_close_with_send", iters_slow, timeout_valgrind * 2)
add_test_case_channel("test_channel_close_with_receive", iters_slow)
add_test_case_sanitize("test_channel_close_with_receive", iters_slow)
add_test_case_valgrind("test_channel_close_with_receive", iters_slow, timeout_valgrind * 2)
add_test_case_channel("serialize", iters_one)
add_test_case_sanitize("serialize", iters_one)
add_test_case_valgrind("serialize", iters_one)
add_test_case_channel("test_correctness", iters_one,timeout_channel * 15)
add_test_case_sanitize("test_correctness", iters_one,timeout_sanitize * 50)
add_test_case_valgrind("test_correctness", iters_one,timeout_valgrind * 50)


# Score distribution
point_breakdown = [
    
    (1, ["channel_test_initialization"]),           #test number 0
    (1, ["sanitize_test_initialization"]),          #test number 1
    (1, ["valgrind_test_initialization"]),          #test number 2
    (1, ["channel_test_Free"]),                     #test number 3
    (1, ["sanitize_test_Free"]),                    #test number 4
    (1, ["valgrind_test_Free"]),                    #test number 5
    (3, ["channel_test_send_correctness"]),         #test number 6
    (3, ["sanitize_test_send_correctness"]),        #test number 7
    (3, ["valgrind_test_send_correctness"]),        #test number 8
    (3, ["channel_test_receive_correctness"]),      #test number 9
    (3, ["sanitize_test_receive_correctness"]),     #test number 10
    (3, ["valgrind_test_receive_correctness"]),     #test number 11
    (3, ["channel_test_overall_send_receive"]),     #test number 12
    (4, ["sanitize_test_overall_send_receive"]),    #test number 13
    (4, ["valgrind_test_overall_send_receive"]),    #test number 14
    (3, ["channel_test_cpu_utilization_send"]),     #test number 15
    (4, ["sanitize_test_cpu_utilization_send"]),    #test number 16
    (4, ["valgrind_test_cpu_utilization_send"]),    #test number 17
    (3, ["channel_test_cpu_utilization_receive"]),  #test number 18
    (4, ["sanitize_test_cpu_utilization_receive"]), #test number 19
    (4, ["valgrind_test_cpu_utilization_receive"]), #test number 20

    
    (4, ["channel_test_channel_close_with_send"]),  #test number 21
    (4, ["sanitize_test_channel_close_with_send"]), #test number 22
    (4, ["valgrind_test_channel_close_with_send"]), #test number 23
    (4, ["channel_test_channel_close_with_receive"]),#test number 24
    (4, ["sanitize_test_channel_close_with_receive"]),#test number 25
    (4, ["valgrind_test_channel_close_with_receive"]),#test number 26

    
    (1, ["channel_serialize"]),                  #test number 27
    (1, ["sanitize_serialize"]),                 #test number 28
    (1, ["valgrind_serialize"]),                 #test number 29
    (5, ["channel_test_correctness"]),              #test number 30
    (6, ["sanitize_test_correctness"]),             #test number 31
    (5, ["valgrind_test_correctness"]),             #test number 32
    
]

def print_success(test):
    print("****SUCCESS: {}****".format(test))

def print_failed(test):
    print("****FAILED: {}****".format(test))

def make_assignment():
    args = ["make", "clean", "all"]
    try:
        subprocess.check_output(args, stderr=subprocess.STDOUT, timeout=timeout_make)
        return True
    except subprocess.CalledProcessError as e:
        print_failed("make")
        print(e.output.decode())
    except subprocess.TimeoutExpired as e:
        print_failed("make")
        print("Failed to compile within {} seconds".format(e.timeout))
    except KeyboardInterrupt:
        print_failed("make")
        print("User interrupted compilation")
    except:
        print_failed("make")
        print("Unknown error occurred")
    return False

def check_global_variables():
    global_variables = []
    for name in ["helper",]:
        error = ""
        args = ["nm", "-f", "posix", "{}.o".format(name)]
        try:
            output = subprocess.check_output(args, stderr=subprocess.STDOUT).decode()
            for line in output.splitlines():
                if re.search(" [BbCcDdGgSsVvWw] ", line):
                    global_variables.append("{}.c: {}".format(name, line.split(" ", 1)[0]))
        except subprocess.CalledProcessError as e:
            error = e.output.decode()
        except KeyboardInterrupt:
            error = "User interrupted global variable test"
        except:
            error = "Unknown error occurred"
        if error != "":
            print_failed("check_global_variables")
            print(error)
            return False
    if len(global_variables) > 0:
        print_failed("check_global_variables")
        print("You are not allowed to use global variables in this assignment:")
        for global_variable in global_variables:
            print(global_variable)
        return False
    return True

def grade():
    if len(sys.argv) == 2:
        result = {}
        test_num = int(sys.argv[1])
        # Run make on the assignment
        if make_assignment() and check_global_variables():
            # Run test cases
            test, config =  list(test_cases.items())[test_num]
            print("Running Test ", test)
            try:
                output = subprocess.check_output(config["args"], stderr=subprocess.STDOUT, timeout=config["timeout"]).decode()
                if "ALL TESTS PASSED" in output:
                    result[test] = True
                    print_success(test)
                else:
                    result[test] = False
                    print_failed(test)
                    error_log = "error_{}.log".format(test)
                    with open(error_log, "w") as fp:
                        fp.write(output)
                    print("See {} for error details".format(error_log))
            except subprocess.CalledProcessError as e:
                result[test] = False
                print_failed(test)
                if e.returncode < 0:
                    if -e.returncode == signal.SIGSEGV:
                        print("Segmentation fault (core dumped)")
                    else:
                        print("Died with signal {}".format(-e.returncode))
                error_log = "error_{}.log".format(test)
                with open(error_log, "w") as fp:
                    fp.write(e.output.decode())
                print("See {} for error details".format(error_log))
            except subprocess.TimeoutExpired as e:
                result[test] = False
                print_failed(test)
                print("Failed to complete within {} seconds".format(e.timeout))
            except KeyboardInterrupt:
                result[test] = False
                print_failed(test)
                print("User interrupted test")
            except:
                result[test] = False
                print_failed(test)
                print("Unknown error occurred")

        # Calculate score
        score = 0
        total_possible = 0
        for points, tests in point_breakdown:
            if all(map(lambda test : test in result and result[test], tests)):
                score += points
            total_possible += points
        return (score, total_possible)
    else:
        result = {}
        # Run make on the assignment
        if make_assignment() and check_global_variables():
            # Run test cases
            for test, config in test_cases.items():
                try:
                    output = subprocess.check_output(config["args"], stderr=subprocess.STDOUT, timeout=config["timeout"]).decode()
                    if "ALL TESTS PASSED" in output:
                        result[test] = True
                        print_success(test)
                    else:
                        result[test] = False
                        print_failed(test)
                        error_log = "error_{}.log".format(test)
                        with open(error_log, "w") as fp:
                            fp.write(output)
                        print("See {} for error details".format(error_log))
                except subprocess.CalledProcessError as e:
                    result[test] = False
                    print_failed(test)
                    if e.returncode < 0:
                        if -e.returncode == signal.SIGSEGV:
                            print("Segmentation fault (core dumped)")
                        else:
                            print("Died with signal {}".format(-e.returncode))
                    error_log = "error_{}.log".format(test)
                    with open(error_log, "w") as fp:
                        fp.write(e.output.decode())
                    print("See {} for error details".format(error_log))
                except subprocess.TimeoutExpired as e:
                    result[test] = False
                    print_failed(test)
                    print("Failed to complete within {} seconds".format(e.timeout))
                except KeyboardInterrupt:
                    result[test] = False
                    print_failed(test)
                    print("User interrupted test")
                except:
                    result[test] = False
                    print_failed(test)
                    print("Unknown error occurred")

        # Calculate score
        score = 0
        total_possible = 0
        for points, tests in point_breakdown:
            if all(map(lambda test : test in result and result[test], tests)):
                score += points
            total_possible += points
        return (score, total_possible)

if __name__ == "__main__":
    score, total_possible = grade()
    print("Final score: {}".format(score))
