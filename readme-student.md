
# Project README file

This is **PROJECT 3: IPC** Readme file.

**********
## Project Description
This project is divided into two parts
Part#1 Implementation of proxy and callback based on libcurl interface
Part#2 Implementation of IPC channels between proxy process and cache daemon

__Part 1__
# Description
In part#1, the implementation is around -
1. Setup proxy for gfserver (_webproxy.c_)
2. writing a callback function (_handle_with_curl.c_) using  libcurl's “easy” C interface.
Components of _handle_with_curl.c_
#a _writedata_ utility function
It is used to write fetched file contents into memory and is based on curl example (https://curl.haxx.se/libcurl/c/getinmemory.html)
#b _handle_with_curl_ callback function
Based on libcurl's 'easy' C interface (http protocol), this function is used to download web content in memory and convert them into server response (getfile protocol)

# Problems and Progress log
1. Issue -: "error": "{\"msg\": \"The total output length exceeded the limit of 65536 bytes.\"}"
Solution -: Reference to url2file.c example on https://curl.haxx.se/libcurl/c/example.html helped fix this issue
2. Issue -: "Tests that the server properly handles requests for non-existent files"
Solution -: Piazza thread #717 helped understand AWS error responses and fix this Issue
3. Issue -: multiple memory leaks during execution
Solution -: Implementation of clean-ups in curl and freeing previous memory allocation helped fix this Issue
4. Issue -: Multiple issues in downloaded files
Solution -: Piazza post #729 and shared script to verify contents of downloaded files helped identify bug in writedata utility function where data size was getting doubled. Fix this bug helped resolve this issue

__Part 2__
# Description
In part#2, the implementation is around setting up IPC between two processes - Proxy Process (P1) and Cache Daemon (P2).
There are 2 different communication modes between these two processes - command channel and data channel. Data channel is handled via shared memory implementation while Command channel is handled via message queues.
#a Proxy process
Key implementation include -
1. Here the number of shared memory segments and their size is defined
2. Creation and destruction of shared Memory and its access control using semaphore
3. Signal handling
4. Crash handling

#b Cache daemon
Key implementation include -
1. Multi-threaded (boss-worker) setup for fulfilling proxy requests via shared memory
2. Cache cleanup
3. Signal handling
4. Crash handling

Link to the overall design diagram is shown here- https://drive.google.com/drive/folders/1zEu65e4W9BO_CwNOXK6MYtwqYep4ii77?usp=sharing

# Problems and Progress log
Struggled with part#2 and have been working until last moment to submit the assignment.

**********
## Known Bugs/Issues/Limitations
1. Part#2 is not fully functional.
2. AddressSanitizer: heap-buffer-overflow error during shared memory segment initialization.

**********
## References
__Part1__
1. Project README documentation
2. Office hours from week 9 and 10
3. Piazza threads - #717, #729, #780
4. How to approach and use libcurl as an absolute beginner (https://www.youtube.com/watch?v=ItbpJ51VvS0)
5. A Beginner’s Guide to LibCurl (https://www.hackthissite.org/articles/read/1078)
6. Beginner's code examples on libcurl implementation (https://curl.haxx.se/libcurl/c/example.html)
7. Curl example of using memory (https://curl.haxx.se/libcurl/c/getinmemory.html)
8. S3 error responses (https://docs.aws.amazon.com/AmazonS3/latest/API/ErrorResponses.html#ErrorCodeList)

__Part2__
1. Project README documentation
2. Office hours from week 9 and 10
3. Lectures P3L3 and P3L2 on Inter Process Communication and Memory Management respectively
4. Piazza threads - #717, #784, #801
5. Michael Kerrisk's youtube vidoe on Intro to Linux IPC (https://www.youtube.com/watch?v=vU2HDf5ZhO4)
6. System V IPC examples (http://www.tldp.org/LDP/lpg/node21.html)
7. Message Queue example (https://www.youtube.com/watch?v=i0XUbhIBbEc)
8. Mutexes and Semaphore examples (https://www.youtube.com/watch?v=RPtsz0Psd2M&feature=youtu.be)
