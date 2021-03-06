#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "core.h"
#include "state.h"
#include "myshlex.h"
#include "mysh.tab.h"

int cd_internal(int argc, char **argv);
int exit_sh(int argc, char **argv);

/* Shell state initialization. */
void init_pwd();
void init() {
	/* pwd setup */
	init_pwd();

	/* internal commands */
	add_intern_cmd("cd", cd_internal);
	add_intern_cmd("exit", exit_sh);
}

/* Gets path of the current working directiory. */
char *get_cwd_path() {
	char *buf = malloc(sizeof (char) * PATH_MAX);
	ERR_EXIT(buf == NULL);

	char *res = getcwd(buf, PATH_MAX);
	if (res == NULL) {
		free(buf);
		ERR_EXIT(1);
	};

	return (res);
}

void init_pwd() {
	char *res = get_cwd_path();

	set_var("PWD", res, true);
	set_var("OLD_PWD", NULL, true);

	free(res);
}

/* Composes current prompt with the current path. */
char *get_prompt() {
	char *pwd = get_var("PWD");
	char *pname = "mysh:";

	char *prompt = malloc((strlen(pwd) + strlen(pname) + 3));
	strcpy(prompt, pname);
	strcat(prompt, pwd);
	strcat(prompt, "> ");

	free(pwd);

	return (prompt);
}

/* Indicates if readline is in its main cycle. */
bool rl_processing;

/* Signal handler for SIGINT. */
void sigint_handler_ia(int signo) {
	if (signo != SIGINT) {
		return;
	}

	// prints a new line
	rl_crlf();

	// prints the prompt if needed
	if (rl_processing) {
		rl_on_new_line_with_prompt();
	}
	else {
		rl_on_new_line();
	}

	// clear line
	rl_replace_line("", 0);

	rl_redisplay();
}

/* Signal handler setup */
void set_sigaction() {
	struct sigaction sa;
	sa.sa_handler = sigint_handler_ia;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART; // necessary for working child signal no

	int sa_res = sigaction(SIGINT, &sa, NULL);
	ERR_EXIT(sa_res == -1); // invalid behavior
}

int parse_string(char *cmd_string);

/* Interactive mode. */
int run_interactive() {
	init();

	rl_processing = false;

	set_sigaction();

	char *buffer = NULL;
	char *prompt = get_prompt();
	while ((buffer = readline(prompt)) != NULL) {
		rl_processing = true;

		if (strlen(buffer) > 0) {
			add_history(buffer);
		}

		// processes and executes the command line
		parse_string(buffer);
		free(buffer);

		free(prompt);
		prompt = get_prompt();

		rl_processing = false;
	}

	// end of input
	free(prompt);
	write(1, "\n", 1);

	int ret_val = get_retval();
	reset_state();

	return (ret_val);
}

/* File mode. */
int run_file(char *file_name) {
	init();

	FILE *fd = fopen(file_name, "r");
	if (fd == NULL) {
		err(1, NULL);
	}

	// process and execute commands
	yyin = fd;
	int parse_val = yyparse();
	fclose(fd);

	int ret_val = get_retval();
	reset_state();
	if (parse_val > 0) {
		return (SYNTAX_ERR);
	}

	return (ret_val);
}

/* String mode. */
int run_string_cmd(char *cmds) {
	init();

	parse_string(cmds);

	int ret_val = get_retval();
	reset_state();
	return (ret_val);
}

/* Processes one command string. */
int parse_string(char *cmd_string) {
	int len;
	if ((len = strlen(cmd_string)) == 0) {
		return (0);
	}

	char *buffer = malloc(len + 2);
	ERR_EXIT(buffer == NULL);

	strcpy(buffer, cmd_string);
	strcat(buffer, "\n");

	YY_BUFFER_STATE buffer_state = yy_scan_string(buffer);
	int parse_ret = yyparse();
	yy_delete_buffer(buffer_state);

	free(buffer);

	if (parse_ret > 0) {
		set_retval(SYNTAX_ERR);
		return (SYNTAX_ERR);
	}

	return (get_retval());
}

/* Internal command, exits shell. */
int exit_sh(int argc, char **argv) {
	if (argc > 1) {
		warn("Syntax error: unused parameters %s, ...", argv[1]);
		return (SYNTAX_ERR); // treated as syntax error
	}

	exit(get_retval());
}

/* Internal command - changes directory. */
int cd_internal(int argc, char **argv) {
	char *dir = NULL;

	switch (argc) {
		case 1:
			// destination is home dir
			dir = strdup(getenv("HOME"));
			ERR_EXIT(dir == NULL);
			break;
		case 2:
			// destination is previous dir
			if (strcmp(argv[1], "-") == 0) {
				dir = get_var("OLDPWD");
			}
			else {
				// destination is a path
				dir = strdup(argv[1]);
				ERR_EXIT(dir == NULL);
			}

			break;
		default:
			fprintf(stderr, "usage: cd <dir>\n");
			return (SYNTAX_ERR);
	}

	/*
	 * If this line is reached, dir is either a valid path
	 * or it should be OLDPWD, but the value is not set.
	 */
	if (dir == NULL) {
		fprintf(stderr, "error: OLDPWD is not set\n");
		return (1);
	}

	// try to change directory
	int dir_res = chdir(dir);

	if (dir_res == -1) {
		switch (errno) {
			case ENOENT:
				fprintf(stderr,
					"cd: %s: No such file or directory\n",
					dir);
				return (1);
			default:
				ERR_EXIT(1);
		}
	}

	// set environmental variables
	char *pwd = get_var("PWD");
	set_var("OLDPWD", pwd, true);

	char *new_pwd = get_cwd_path();
	set_var("PWD", new_pwd, true);

	free(new_pwd);
	free(pwd);
	free(dir);

	return (0);
}
