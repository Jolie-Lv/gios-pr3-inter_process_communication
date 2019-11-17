cmake_minimum_required(VERSION 3.9)
project(cache)

set(CMAKE_CXX_STANDARD 11)

add_executable(cache
        cache-student.h
        gfserver.h
        handle_with_cache.c
        locals.txt
        Makefile
        shm_channel.c
        shm_channel.h
        simplecache.c
        simplecache.h
        simplecached.c
        steque.c
        steque.h
        webproxy.c
        workload.txt)
