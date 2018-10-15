#include "file_storage.h"
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <memory.h>
#include <stdlib.h>
#include <assert.h>


void catch_function()
{
    printf("An error occurred\n");
    exit(EXIT_FAILURE);
}

int main()
{
    // A way to throw an exception in C
    if (signal(SIGUSR1, catch_function) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }

    initFileStorage();
    createFs(1 << 17);

    char command[1 << 20];
    char** strings = malloc(sizeof(char*) * 51);
    for (int i = 0; i < 51; ++i)
    {
        strings[i] = malloc(1 << 10);
        strings[i][0] = '\0';
    }
    while (true)
    {
        assert(scanf("%s", command));
        if (strcmp(command, "quit") == 0)
        {
            break;
        }
        else if (strcmp(command, "ls") == 0)
        {
            assert(scanf("%s", command));
            ls(command, 50, strings);
            int i = -1;
            while (strcmp(strings[++i], "") != 0)
            {
                printf("%s ", strings[i]);
            }
            printf("\n");
        }
        else if (strcmp(command, "mkdir") == 0)
        {
            assert(scanf("%s", command));
            mkdir(command);
        }
        else if (strcmp(command, "touch") == 0)
        {
            //
        }
        else if (strcmp(command, "rm") == 0)
        {
            raise(SIGUSR1);
            //
        }
    }

    for (int i = 0; i < 51; ++i)
    {
        free(strings[i]);
    }
    free(strings);
    tearDownFileStorage();
    return EXIT_SUCCESS;
}
