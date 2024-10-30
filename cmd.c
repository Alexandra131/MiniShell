// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (dir == NULL)
		return false;

	if (chdir(dir->string) == -1)
		return false;

	return true;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	exit(SHELL_EXIT);
	return SHELL_EXIT; /* TODO: Replace with actual exit code. */
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */
	if (s == NULL)
		return shell_exit();

	char *dir = malloc(512);

	char *ret = getcwd(dir, 512);

	if (ret == NULL)
		return shell_exit();

	/* TODO: If builtin command, execute the command. */
	if (strcmp(s->verb->string, "cd") == 0) {
		if (s->params == NULL)
			return shell_cd(NULL);

		int stdin_fileno = dup(STDIN_FILENO);
		char stdin_filename[512];

		int stdout_fileno = dup(STDOUT_FILENO);
		char stdout_filename[512];

		int stderr_fileno = dup(STDERR_FILENO);
		char stderr_filename[512];

		if (s->in != NULL) {
			snprintf(stdin_filename, sizeof(stdin_filename) - 1, "%s", s->in->string);

			if (s->in->next_part)
				strcat(stdin_filename, get_word(s->in->next_part));
		}

		if (s->out != NULL) {
			snprintf(stdout_filename, sizeof(stdout_filename) - 1, "%s", s->out->string);

			if (s->out->next_part)
				strcat(stdout_filename, get_word(s->out->next_part));
		}

		if (s->err != NULL) {
			snprintf(stderr_filename, sizeof(stderr_filename) - 1, "%s", s->err->string);

			if (s->err->next_part)
				strcat(stderr_filename, get_word(s->err->next_part));
		}

		if (s->in != NULL) {
			int fd = open(stdin_filename, O_RDONLY);

			if (fd == -1)
				return shell_exit();

			if (dup2(fd, STDIN_FILENO) == -1)
				return shell_exit();

			close(fd);
		}

		if (s->out != NULL && s->err != NULL) {
			int fd1 = open(stdout_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

			int fd2 = open(stderr_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (fd1 == -1 || fd2 == -1)
				return shell_exit();

			if (dup2(fd1, STDOUT_FILENO) == -1)
				return shell_exit();

			if (dup2(fd2, STDERR_FILENO) == -1)
				return shell_exit();

			close(fd1);
			close(fd2);
		} else if (s->out != NULL && s->err == NULL) {
			if (s->io_flags == IO_REGULAR) {
				int fd = open(stdout_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDOUT_FILENO) == -1)
					return shell_exit();

				close(fd);
			} else {
				int fd = open(stdout_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDOUT_FILENO) == -1)
					return shell_exit();

				close(fd);
			}
		} else if (s->out == NULL && s->err != NULL) {
			if (s->io_flags == IO_REGULAR) {
				int fd = open(stderr_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDERR_FILENO) == -1)
					return shell_exit();

				close(fd);
			} else {
				int fd = open(stderr_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDERR_FILENO) == -1)
					return shell_exit();

				close(fd);
			}
		}

		dup2(stdin_fileno, STDIN_FILENO);
		dup2(stdout_fileno, STDOUT_FILENO);
		dup2(stderr_fileno, STDERR_FILENO);

		return shell_cd(s->params);
	}

	if (strcmp(s->verb->string, "exit") == 0)
		return shell_exit();
	else if (strcmp(s->verb->string, "quit") == 0)
		return shell_exit();


	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (s->verb->next_part != NULL) {
		if (setenv(s->verb->string, get_word(s->verb->next_part->next_part), 1) == -1)
			return shell_exit();

		return true;
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	pid_t fork_ret = fork();

	if (fork_ret < 0)
		return shell_exit();

	if (fork_ret == 0) {
		int argc = 0;
		char **argv = get_argv(s, &argc);

		char stdin_filename[512];
		char stdout_filename[512];
		char stderr_filename[512];

		if (s->in != NULL) {
			snprintf(stdin_filename, sizeof(stdin_filename) - 1, "%s", s->in->string);

			if (s->in->next_part)
				strcat(stdin_filename, get_word(s->in->next_part));
		}

		if (s->out != NULL) {
			snprintf(stdout_filename, sizeof(stdout_filename) - 1, "%s", s->out->string);

			if (s->out->next_part)
				strcat(stdout_filename, get_word(s->out->next_part));
		}

		if (s->err != NULL) {
			snprintf(stderr_filename, sizeof(stderr_filename) - 1, "%s", s->err->string);

			if (s->err->next_part)
				strcat(stderr_filename, get_word(s->err->next_part));
		}

		if (s->in != NULL) {
			int fd = open(stdin_filename, O_RDONLY);

			if (fd == -1)
				return shell_exit();

			if (dup2(fd, STDIN_FILENO) == -1)
				return shell_exit();

			close(fd);
		}

		if (s->out != NULL && s->err != NULL) {
			int fd1 = open(stdout_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

			int fd2 = open(stderr_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (fd1 == -1 || fd2 == -1)
				return shell_exit();

			if (dup2(fd1, STDOUT_FILENO) == -1)
				return shell_exit();

			if (dup2(fd2, STDERR_FILENO) == -1)
				return shell_exit();

			close(fd1);
			close(fd2);
		} else if (s->out != NULL && s->err == NULL) {
			if (s->io_flags == IO_REGULAR) {
				int fd = open(stdout_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDOUT_FILENO) == -1)
					return shell_exit();

				close(fd);
			} else {
				int fd = open(stdout_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDOUT_FILENO) == -1)
					return shell_exit();

				close(fd);
			}
		} else if (s->out == NULL && s->err != NULL) {
			if (s->io_flags == IO_REGULAR) {
				int fd = open(stderr_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDERR_FILENO) == -1)
					return shell_exit();

				close(fd);
			} else {
				int fd = open(stderr_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

				if (fd == -1)
					return shell_exit();

				if (dup2(fd, STDERR_FILENO) == -1)
					return shell_exit();

				close(fd);
			}
		}

		execvp(argv[0], argv);
		printf("Execution failed for '%s'\n", get_word(s->verb));

		return shell_exit();
	}

	if (fork_ret > 0) {
		int status;

		waitpid(fork_ret, &status, 0);

		return !status;
	}

	return shell_exit(); /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	pid_t fork_ret1 = fork();

	if (fork_ret1 < 0)
		return shell_exit();

	if (fork_ret1 == 0) {
		int ret = parse_command(cmd1, level + 1, father);

		exit(ret);
	}

	pid_t fork_ret2 = fork();

	if (fork_ret2 < 0)
		return shell_exit();

	if (fork_ret2 == 0) {
		int ret = parse_command(cmd2, level + 1, father);

		exit(ret);
	}

	if (fork_ret1 > 0 && fork_ret2 > 0) {
		int status1;

		waitpid(fork_ret1, &status1, 0);

		int status2;

		waitpid(fork_ret2, &status2, 0);

		return !status1 && !status2;
	}

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int rw_fd[2];

	if (pipe(rw_fd) == -1)
		return shell_exit();

	pid_t fork_ret1 = fork();

	if (fork_ret1 < 0)
		return shell_exit();

	if (fork_ret1 == 0) {
		close(rw_fd[READ]);

		if (dup2(rw_fd[WRITE], STDOUT_FILENO) == -1)
			return shell_exit();

		int ret = parse_command(cmd1, level + 1, father);

		exit(ret);
	}

	pid_t fork_ret2 = fork();

	if (fork_ret2 < 0)
		return shell_exit();

	if (fork_ret2 == 0) {
		close(rw_fd[WRITE]);

		if (dup2(rw_fd[READ], STDIN_FILENO) == -1)
			return shell_exit();

		int ret = parse_command(cmd2, level + 1, father);

		exit(ret);
	}

	if (fork_ret1 > 0 && fork_ret2 > 0) {
		int status;

		close(rw_fd[READ]);
		waitpid(fork_ret1, &status, 0);

		close(rw_fd[WRITE]);
		waitpid(fork_ret2, &status, 0);

		return status;
	}

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	if (c == NULL)
		return shell_exit();

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		int ret = parse_simple(c->scmd, level, father);

		return ret; /* TODO: Replace with actual exit code of command. */
	}

	switch (c->op) {
	case OP_SEQUENTIAL: {
		/* TODO: Execute the commands one after the other. */
		int ret = parse_command(c->cmd1, level + 1, c);

		if (ret >= 0)
			ret = parse_command(c->cmd2, level + 1, c);

		return ret;
	}

	case OP_PARALLEL: {
		/* TODO: Execute the commands simultaneously. */
		int ret = run_in_parallel(c->cmd1, c->cmd2, level, c);

		return ret;
	}

	case OP_CONDITIONAL_NZERO: {
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		int ret = parse_command(c->cmd1, level + 1, c);

		if (ret == 0)
			ret = parse_command(c->cmd2, level + 1, c);

		return ret;
	}

	case OP_CONDITIONAL_ZERO: {
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		int ret = parse_command(c->cmd1, level + 1, c);

		if (ret != 0)
			ret = parse_command(c->cmd2, level + 1, c);

		return ret;
	}

	case OP_PIPE: {
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		int ret = run_on_pipe(c->cmd1, c->cmd2, level, c);

		return ret;
	}

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
