# CMake generated Testfile for 
# Source directory: /Users/emxy_yang/Working_dir/Project/ESet
# Build directory: /Users/emxy_yang/Working_dir/Project/ESet/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[Test_1]=] "/bin/sh" "-c" "/Users/emxy_yang/Working_dir/Project/ESet/build/Test_Exe_1 </Users/emxy_yang/Working_dir/Project/ESet/test/1.in> /Users/emxy_yang/Working_dir/Project/ESet/build/1.out && /opt/homebrew/bin/cmake -E compare_files --ignore-eol /Users/emxy_yang/Working_dir/Project/ESet/build/1.out /Users/emxy_yang/Working_dir/Project/ESet/test/1.ans")
set_tests_properties([=[Test_1]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/emxy_yang/Working_dir/Project/ESet/CMakeLists.txt;23;add_test;/Users/emxy_yang/Working_dir/Project/ESet/CMakeLists.txt;0;")
add_test([=[Test_2]=] "/bin/sh" "-c" "/Users/emxy_yang/Working_dir/Project/ESet/build/Test_Exe_2 </Users/emxy_yang/Working_dir/Project/ESet/test/2.in> /Users/emxy_yang/Working_dir/Project/ESet/build/2.out && /opt/homebrew/bin/cmake -E compare_files --ignore-eol /Users/emxy_yang/Working_dir/Project/ESet/build/2.out /Users/emxy_yang/Working_dir/Project/ESet/test/2.ans")
set_tests_properties([=[Test_2]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/emxy_yang/Working_dir/Project/ESet/CMakeLists.txt;23;add_test;/Users/emxy_yang/Working_dir/Project/ESet/CMakeLists.txt;0;")
