/*
 * [CODEGEN] tests/harness/div0.c
 *
 * Division by zero must fail deterministically: v0.1 emits no
 * zero-division guard, so native integer division by a zero-valued
 * variable dies with SIGFPE. This harness forks, execs `pith run` on
 * tests/codegen_div0.pi, and verifies the exact signal — proving the
 * crash propagates predictably instead of hanging or producing a
 * wrong result.
 *
 * Expected behavior: exit 0 (the child died by SIGFPE).
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *pith = argc > 1 ? argv[1] : "./pith";
    const char *script = argc > 2 ? argv[2] : "tests/codegen_div0.pi";

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execl(pith, pith, "run", script, (char *)NULL);
        _exit(127);   /* exec failed */
    }

    int st;
    if (waitpid(pid, &st, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFSIGNALED(st) && WTERMSIG(st) == SIGFPE) {
        printf("div0: deterministic SIGFPE ok\n");
        return 0;
    }
    if (WIFSIGNALED(st)) {
        fprintf(stderr, "div0: child died by signal %d, expected %d\n",
                WTERMSIG(st), SIGFPE);
    } else if (WIFEXITED(st)) {
        fprintf(stderr, "div0: child exited with %d, expected SIGFPE\n",
                WEXITSTATUS(st));
    } else {
        fprintf(stderr, "div0: unexpected child state\n");
    }
    return 1;
}
