#ifndef _minunit_h
#define _minunit_h

#include <stdio.h>
#include <stdlib.h>

#define mu_suite_start() char *message = NULL

#define debug printf
#define log_err(...) do{printf("[ERROR](%s,%u):", __FILE__, __LINE__); printf(__VA_ARGS__); \
    printf("\n");}while(0)

#define mu_assert(test, message) if (!(test)) { log_err(message); return message; }
#define mu_run_test(test) debug("\n-----%s ", " " #test); \
    message = test(); tests_run++; if (message) return message;

#define RUN_TESTS(name) int main(int argc, char *argv[]) {\
    argc = 1; \
    debug("----- RUNNING: %s", argv[0]);\
        printf("----\nRUNNING: %s\n", argv[0]);\
        char *result = name();\
        if (result != 0) {\
            printf("FAILED: %s\n", result);\
        }\
        else {\
            printf("\nALL TESTS PASSED\n");\
        }\
    printf("Tests run: %d\n", tests_run);\
        exit(result != 0);\
    return 0;\
}


int tests_run;

#endif
