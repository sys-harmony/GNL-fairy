#include "gnl_fairy.h"

/*
 * Leak tests are NOT forked: the whole process runs under Valgrind, which
 * inspects every allocation at exit. Each helper therefore reads its file to
 * EOF so that a correct get_next_line frees its static stash (a partial read
 * would legitimately leave a "still reachable" buffer and is not a leak).
 * We also exercise the two failure paths: read() error and malloc() failure,
 * where get_next_line must return NULL without leaking.
 */

static void	drain(int fd) {
	char	*line;

	while ((line = get_next_line(fd)) != NULL)
		free(line);
}

static void	leak_full_with_nl(void) {
	char	path[32];
	int		fd = gnl_tmp_open("alpha\nbeta\ngamma\n", path);

	drain(fd);
	close(fd);
	unlink(path);
}

static void	leak_full_no_nl(void) {
	char	path[32];
	int		fd = gnl_tmp_open("alpha\nbeta\ngamma", path);

	drain(fd);
	close(fd);
	unlink(path);
}

static void	leak_long_line(void) {
	char	big[8192];
	char	path[32];
	int		fd;

	for (size_t i = 0; i < sizeof(big) - 1; i++)
		big[i] = 'a';
	big[sizeof(big) - 1] = '\0';
	fd = gnl_tmp_open(big, path);
	drain(fd);
	close(fd);
	unlink(path);
}

static void	leak_many_lines(void) {
	char	path[32];
	int		fd = gnl_tmp_open("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n", path);

	drain(fd);
	close(fd);
	unlink(path);
}

static void	leak_read_error(void) {
	char	path[32];
	int		fd = gnl_tmp_open("will not be read\n", path);
	char	*line;

	g_read_error_enabled = 1;
	line = get_next_line(fd);
	g_read_error_enabled = 0;
	free(line);
	drain(fd);
	close(fd);
	unlink(path);
}

static void	leak_malloc_fail(void) {
	char	path[32];
	int		fd = gnl_tmp_open("x\ny\nz\n", path);
	char	*line;

	for (int k = 1; k <= 3; k++) {
		g_malloc_count = 0;
		g_malloc_fail_at = k;
		g_malloc_wrap_enabled = 1;
		line = get_next_line(fd);
		g_malloc_wrap_enabled = 0;
		free(line);
	}
	drain(fd);
	close(fd);
	unlink(path);
}

int	main(void) {
	leak_full_with_nl();
	leak_full_no_nl();
	leak_long_line();
	leak_many_lines();
	leak_read_error();
	leak_malloc_fail();
	return 0;
}
