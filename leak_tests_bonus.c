#include "gnl_fairy.h"

/*
 * Bonus leak tests: several descriptors are read concurrently and each must be
 * drained to EOF so the per-fd stash is freed. Run under Valgrind by run.sh.
 */

#define NB_FD 3

static void	drain(int fd) {
	char	*line;

	while ((line = get_next_line(fd)) != NULL)
		free(line);
}

static void	leak_multi_fd_interleaved(void) {
	const char	*content[NB_FD] = {
		"a\nb\nc\n",
		"d\ne\n",
		"f\ng\nh\ni\n"
	};
	char		path[NB_FD][32];
	int			fd[NB_FD];
	char		*line;

	for (int i = 0; i < NB_FD; i++)
		fd[i] = gnl_tmp_open(content[i], path[i]);
	/* read one line from each before draining, to keep stashes live */
	for (int i = 0; i < NB_FD; i++) {
		line = get_next_line(fd[i]);
		free(line);
	}
	for (int i = 0; i < NB_FD; i++) {
		drain(fd[i]);
		close(fd[i]);
		unlink(path[i]);
	}
}

static void	leak_multi_fd_malloc_fail(void) {
	const char	*content[NB_FD] = {
		"11\n22\n",
		"33\n44\n",
		"55\n66\n"
	};
	char		path[NB_FD][32];
	int			fd[NB_FD];
	char		*line;

	for (int i = 0; i < NB_FD; i++)
		fd[i] = gnl_tmp_open(content[i], path[i]);
	g_malloc_count = 0;
	g_malloc_fail_at = 2;
	g_malloc_wrap_enabled = 1;
	for (int i = 0; i < NB_FD; i++) {
		line = get_next_line(fd[i]);
		free(line);
	}
	g_malloc_wrap_enabled = 0;
	for (int i = 0; i < NB_FD; i++) {
		drain(fd[i]);
		close(fd[i]);
		unlink(path[i]);
	}
}

int	main(void) {
	leak_multi_fd_interleaved();
	leak_multi_fd_malloc_fail();
	return 0;
}
