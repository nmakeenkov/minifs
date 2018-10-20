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
    tearDownFileStorage();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    // A way to throw an exception in C
    if (signal(SIGUSR1, catch_function) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }

    if (argc != 2)
    {
        fputs("Wrong arguments", stderr);
        return EXIT_FAILURE;
    }

    initFileStorage(argv[1]);
    createFs(1 << 17);

    char command[1 << 10];
    char contents[1 << 20];
    while (true)
    {
        assert(scanf("%s", command));
        if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0)
        {
            break;
        }
        else if (strcmp(command, "ls") == 0)
        {
            assert(scanf("%s", command));
            char strings[51][NAME_MAX_LENGTH];
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
        else if (strcmp(command, "set_file_contents") == 0)
        {
            assert(scanf("%s %s", command, contents));
            setFileContents(command, contents);
        }
        else if (strcmp(command, "cat") == 0)
        {
            assert(scanf("%s", command));
            cat(command, contents);
            printf("%s\n", contents);
        }
        else if (strcmp(command, "rm") == 0)
        {
            assert(scanf("%s", command));
            rm(command);
        }
        else if (strcmp(command, "rmdir") == 0)
        {
            assert(scanf("%s", command));
            rmdir(command);
        }
        else if (strcmp(command, "ln") == 0)
        {
            assert(scanf("%s", command));
            assert(scanf("%s", contents));
            ln(command, contents);
        }
        else
        {
            fprintf(stderr, "Unknown command %s\n", command);
            continue;
        }
    }

    tearDownFileStorage();
    return EXIT_SUCCESS;
}
