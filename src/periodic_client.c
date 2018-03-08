#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hiredis.h>

int main(int argc, char **argv) {
    redisContext *c;
    redisReply *reply;
	char acbuf[255];
    const char *hostname = (argc > 1) ? argv[1] : "192.168.1.11";
    int port = (argc > 2) ? atoi(argv[2]) : 6379;

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        }
        else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }

	/* Generate Random Seed */
    srand(time(NULL));

    reply = redisCommand(c,"FLUSHALL");
    printf("FLUSHALL: %s\n", reply->str);
	if(0 != strcmp(reply->str, "OK"))
	{
		freeReplyObject(reply);
		printf("Fail to flush database!\n");
		goto client_exit;
	}
	freeReplyObject(reply);

	sprintf(acbuf, "HSET myset:%d element:%d xxx", rand(), rand());
    /* Set a key */
    reply = redisCommand(c, acbuf);
	/* printf("HSET: %lld\n", reply->integer); */
    freeReplyObject(reply);



client_exit:
    /* Disconnects and frees the context */
    redisFree(c);

    return 0;
}
