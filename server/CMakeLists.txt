cmake_minimum_required(VERSION 3.9)
project(server)

set(CMAKE_CXX_STANDARD 11)

add_executable(server
        gfserver.h
        handle_with_curl.c
        handle_with_file.c
        locals.txt
        Makefile
        proxy-student.h
        steque.c
        steque.h
        webproxy.c
        workload.txt)
