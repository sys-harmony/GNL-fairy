#include "gnl_fairy.h"
#include <sys/wait.h>

int		g_malloc_wrap_enabled = 0;
int		g_malloc_count;
int		g_malloc_fail_at;
int		g_read_error_enabled = 0;
int		g_tests_failed = 0;

void	*__wrap_malloc(size_t size) {
	if (g_malloc_wrap_enabled && ++g_malloc_count == g_malloc_fail_at)
		return NULL;
	return __real_malloc(size);
}

ssize_t	__wrap_read(int fd, void *buf, size_t count) {
	if (g_read_error_enabled)
		return -1;
	return __real_read(fd, buf, count);
}

int	all_tests_passed(const int *passed, const size_t num_tests) {
	for (size_t i = 0; i < num_tests; i++) {
		if (!passed[i])
			return 0;
	}
	return 1;
}

void	print_test_results(char *function_name, const size_t num_tests, const char *tests[], const int passed[]) {
	printf("\n========================================\n");
	printf("%s\n", function_name);
	printf("========================================\n");
	for (size_t i = 0; i < num_tests; i++) {
		printf("%s" RESET "  Test %s\n", passed[i] ? GREEN "[OK]" : RED "[KO]", tests[i]);
		if (!passed[i])
			g_tests_failed++;
	}
}

void	error_exit(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

/* ************************************************************************** */
/*                            GNL-specific helpers                            */
/* ************************************************************************** */

/*
 * Writes `content` to a fresh temp file, then reopens it read-only and returns
 * the fd. The path is stored into `path` (caller must hold >= 32 bytes and is
 * responsible for unlink()). The reference output of get_next_line does not
 * depend on BUFFER_SIZE, so the same files validate every buffer size.
 */
int	gnl_tmp_open(const char *content, char *path) {
	int		fd;
	size_t	len;

	strcpy(path, "/tmp/gnl_fairy_XXXXXX");
	fd = mkstemp(path);
	if (fd < 0)
		error_exit("gnl-fairy: mkstemp failed");
	len = strlen(content);
	if (write(fd, content, len) != (ssize_t)len)
		error_exit("gnl-fairy: write failed");
	close(fd);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		error_exit("gnl-fairy: open failed");
	return fd;
}

/*
 * Returns the next expected line as a freshly malloc'd string: every line
 * keeps its trailing '\n', except the final segment when `content` does not
 * end with one. Returns NULL once the cursor reached the end. Advances cursor.
 */
char	*gnl_expected_next(const char **cursor) {
	const char	*s = *cursor;
	const char	*nl;
	const char	*end;
	size_t		n;
	char		*line;

	if (!*s)
		return NULL;
	nl = strchr(s, '\n');
	if (nl)
		end = nl + 1;
	else
		end = s + strlen(s);
	n = (size_t)(end - s);
	line = malloc(n + 1);
	if (!line)
		error_exit("gnl-fairy: malloc failed");
	memcpy(line, s, n);
	line[n] = '\0';
	*cursor = end;
	return line;
}

/*
 * End-to-end scenario: writes `content` to a file, reads it back line by line
 * with get_next_line and compares each line to the expectation, then asserts
 * that get_next_line returns NULL twice at EOF (idempotent end-of-file).
 * Returns 0 on success, 1 on any divergence. Always cleans up the temp file.
 */
int	gnl_check_content(const char *content) {
	char		path[32];
	int			fd;
	const char	*cursor;
	char		*expected;
	char		*got;
	int			i;

	fd = gnl_tmp_open(content, path);
	cursor = content;
	while ((expected = gnl_expected_next(&cursor)) != NULL) {
		got = get_next_line(fd);
		if (!got || strcmp(got, expected) != 0) {
			free(got);
			free(expected);
			close(fd);
			unlink(path);
			return 1;
		}
		free(got);
		free(expected);
	}
	i = 0;
	while (i++ < 2) {
		got = get_next_line(fd);
		if (got) {
			free(got);
			close(fd);
			unlink(path);
			return 1;
		}
	}
	close(fd);
	unlink(path);
	return 0;
}

/* ************************************************************************** */
/*                           forked test runner                               */
/* ************************************************************************** */

static pid_t	xwaitpid(pid_t pid, int *status, int options)
{
	pid_t	ret = waitpid(pid, status, options);
	if (ret == -1)
		error_exit("gnl-fairy: waitpid failed");
	return ret;
}

int	forked_test(void (*test_func)(void)) {
	pid_t	pid;
	int		status;

	fflush(stdout);
	pid = fork();
	if (pid == -1)
		error_exit("gnl-fairy: fork failed");
	if (!pid) {
		alarm(GNL_TIMEOUT);
		test_func();
		exit(EXIT_SUCCESS);
	}
	xwaitpid(pid, &status, 0);
	if (WIFSIGNALED(status))
		return 1;
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		return 1;
	return 0;
}
