cmake_minimum_required(VERSION 3.28)
project(ESet)

set(CMAKE_CXX_STANDARD 20)
include_directories(.)

enable_testing()

set(TEST_CASES "1" "2")

foreach(test_name ${TEST_CASES})
    set(EXE_NAME "Test_Exe_${test_name}")
    add_executable(${EXE_NAME}
            "test/${test_name}.cpp"
            src.hpp)
    # 定义输入、输出和程序生成的临时结果路径
    set(IN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/${test_name}.in")
    set(ANS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/${test_name}.ans")
    set(OUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${test_name}.out")

    # 添加测试
    # 逻辑：运行程序 < in > ans，然后对比 ans 和 out
    add_test(NAME "Test_${test_name}"
            COMMAND /bin/sh -c
            "$<TARGET_FILE:${EXE_NAME}> <${IN_FILE}> ${OUT_FILE} && ${CMAKE_COMMAND} -E compare_files --ignore-eol ${OUT_FILE} ${ANS_FILE}"
    )
endforeach()

add_executable(Private_Test
        src.hpp
        pritest.cpp)
