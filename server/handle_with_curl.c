#include <curl/curl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <printf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <unistd.h>

#include "gfserver.h"
#include "proxy-student.h"


#define BUFSIZE (6200)

typedef struct request
{
    gfcontext_t *ctx;
    char *Data;
    size_t totalsize;
}request;

/* writedata utility function is used to write fetched file contents into memory
 * it is based on curl example (https://curl.haxx.se/libcurl/c/getinmemory.html)
 * */
static size_t writedata(void *ptr, size_t size, size_t nmemb, void *stream)
{
    request *req = (request*) stream;
    size_t totalsize = size * nmemb;
    //req->totalsize=totalsize;
    req->Data = realloc(req->Data, req->totalsize + totalsize + 1);
    if(req->Data == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }
    memcpy(&(req->Data[req->totalsize]), ptr, totalsize);
    req->totalsize += totalsize;
    req->Data[req->totalsize]=0;
    return totalsize;
}

/*
 * handle_with_curl is the suggested name for your curl call back.
 * We suggest you implement your code here.  Feel free to use any
 * other functions that you may need.
 */
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	/*(void) ctx;
	(void) path;
	(void) arg; */
    char buffer[BUFSIZE];
    char *data_dir = arg;
    CURL* curl;
    CURLcode rescurl, resurl, resredir, reswrite, reswritedata;
    //size_t file_len=0, read_len=0;
    size_t bytes_transferred=0;
    ssize_t write_len=0;
    long response =0;
    //struct stat statbuf;
    char errbuf[CURL_ERROR_SIZE];
    errbuf[0] = 0;
    request req;
    req.ctx= ctx;
    req.totalsize=0;
    req.Data=malloc(1);

    fprintf(stdout, "start of handle_with_curl\n");
    fprintf(stdout, "server - %s\n", data_dir);
    fprintf(stdout, "path - %s\n", path);

    if (path == NULL || path[0] == '\0'){
        fprintf(stderr, "path is null");
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    if (arg == NULL){
        fprintf(stderr, "arg is null");
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }

    strncpy(buffer,data_dir, BUFSIZE);
    strncat(buffer,path, BUFSIZE);
    fprintf(stdout, "address- %s\n", buffer);

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    fprintf(stdout, "after curl_easy_init\n");

    if(!curl){
        fprintf(stderr, "Curl initialization error\n");
        return GF_ERROR;
    } else {
        fprintf(stdout, "start processing\n");
        if ((resurl = curl_easy_setopt(curl, CURLOPT_URL, buffer)) != CURLE_OK) {
            fprintf(stderr, "error - %s\n", curl_easy_strerror(resurl));
            return GF_ERROR;
        }
        if ((resurl = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf)) != CURLE_OK) {
            fprintf(stderr, "error - %s\n", curl_easy_strerror(resurl));
            return GF_ERROR;
        }
        if ((resredir = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)) != CURLE_OK) {
            fprintf(stderr, "error - %s\n", curl_easy_strerror(resredir));
            return GF_ERROR;
        }
        if ((reswrite = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata)) != CURLE_OK){
            fprintf(stderr, "error - %s\n", curl_easy_strerror(reswrite));
            return GF_ERROR;
        }
        if ((reswritedata=curl_easy_setopt(curl, CURLOPT_WRITEDATA, &req)) != CURLE_OK){
            fprintf(stderr, "error - %s\n", curl_easy_strerror(reswritedata));
            return GF_ERROR;
            }

        rescurl = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
        fprintf(stdout, "response - %ld, size - %zd\n", response, req.totalsize);

        if ( rescurl!= CURLE_OK) {
            fprintf(stdout, "error in curl_easy_perform\n");
            size_t len = strlen(errbuf);
            fprintf(stderr, "\nlibcurl: (%d) \n", rescurl);
            if (len)
                fprintf(stderr, "%s%s", errbuf,
                        ((errbuf[len - 1] != '\n') ? "\n" : ""));
            else
                fprintf(stderr, "%s\n", curl_easy_strerror(rescurl));

            if (rescurl == CURLE_URL_MALFORMAT || rescurl == CURLE_REMOTE_FILE_NOT_FOUND || rescurl == CURLE_READ_ERROR ||
                rescurl == CURLE_FILE_COULDNT_READ_FILE)
                return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
            else
                return gfs_sendheader(ctx, GF_ERROR, 0);
        }

        if(response >= 400){
            fprintf(stdout, "file not found\n");
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }

        fprintf(stdout, "success in curl_easy_perform\n");
        ssize_t  recvheader = gfs_sendheader(ctx, GF_OK, req.totalsize);
        fprintf(stdout, "send OK, response - %zd\n", recvheader);

        /* Calculating the file size */
         /*if (0 > fstat(req.totalsize, &statbuf)) {
            return SERVER_FAILURE;
        }
        file_len = (size_t) statbuf.st_size;
        fprintf(stdout, "file length = %ld\n", file_len); */

        /* Sending the file contents chunk by chunk. */

        bytes_transferred = 0;
        while (bytes_transferred < req.totalsize) {
            fprintf(stdout, "total size - %zd\n", req.totalsize);
            size_t bytes_left = req.totalsize - bytes_transferred;
            write_len = gfs_send(ctx, &req.Data[bytes_transferred], bytes_left);
            fprintf(stdout, "transferred size - %zd, left size - %zd, written size - %zd\n", bytes_transferred, bytes_left, write_len);
            fprintf(stdout, "write success\n");
            /*if (write_len != read_len) {
                fprintf(stderr, "handle_with_file write error\n");
                return SERVER_FAILURE;
            }*/
            bytes_transferred += write_len;
            fprintf(stdout, "transfer success\n");
        }

        free(req.Data);
        curl_easy_cleanup(curl);
        curl_global_cleanup();

        return bytes_transferred;


        /* not implemented -- yet */
        /* errno = ENOSYS;
        return -1; */


    }

}


/*
 * The original sample uses handle_with_file.  We add a wrapper here so that
 * you can use this in place of the existing invocation of handle_with_file.
 * 
 * Feel free to use handle_with_curl directly.
 */
ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}