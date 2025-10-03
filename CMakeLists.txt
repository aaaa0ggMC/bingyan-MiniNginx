cmake_minimum_required(VERSION 3.10)
project(MiniNginx)

set(HEADER_DIR "./headers")

# ALIB-G3的部分文件（同时删减了部分功能，比如对glm实体的输出），提供日志处理和计时功能，完整的版本见UL
# (因为ALIB-G3是UL的一个字项目): https://github.com/aaaa0ggMC/UnlimitedLife-Linux 
aux_source_directory("./src/alib-g3" ALIB_G3_PARTIAL)
# MINI_NGINX的主要文件：指的是main.cpp,其他文件一般要打包进文件夹哈哈
aux_source_directory("./src" MINI_NGINX_MAIN)
aux_source_directory("./src/tests" MINI_NGINX_TEST)
aux_source_directory("./src/modules" MINI_NGINX_MODULES)

if(CI_BUILD)
set(build_dir "${CMAKE_SOURCE_DIR}/CBuild")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${build_dir})
endif()

add_executable(mini_nginx 
    ${ALIB_G3_PARTIAL}
    ${MINI_NGINX_MAIN}
    ${MINI_NGINX_MODULES}
)
target_include_directories(mini_nginx PUBLIC ${HEADER_DIR})
target_compile_options(mini_nginx PUBLIC -std=c++26)

add_executable(mini_nginx_tests
    ${ALIB_G3_PARTIAL}
    ${MINI_NGINX_MAIN}
    ${MINI_NGINX_MODULES}
    ${MINI_NGINX_TEST}
)
target_include_directories(mini_nginx_tests PUBLIC ${HEADER_DIR})
target_link_libraries(mini_nginx_tests PRIVATE gtest gtest_main)
target_compile_options(mini_nginx_tests PUBLIC -DMNGINX_TESTING -std=c++26)