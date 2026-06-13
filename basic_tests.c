#include "gnl_fairy.h"

#ifndef VERBOSE
# define VERBOSE 0
#endif

/*
 * Every scenario writes a known buffer to a temp file and reads it back line
 * by line, comparing against the reference split (see gnl_check_content).
 * The expected output is identical for every BUFFER_SIZE, so run.sh recompiles
 * this file for several sizes (1, 42, 9999, 10000000) and runs it each time.
 */

/* ************************************************************************** */
/*                              tiny inputs                                   */
/* ************************************************************************** */

static void	empty_file_test(void) {
	if (gnl_check_content(""))
		abort();
}

static void	one_char_no_nl_test(void) {
	if (gnl_check_content("a"))
		abort();
}

static void	one_char_nl_test(void) {
	if (gnl_check_content("a\n"))
		abort();
}

static void	lone_nl_test(void) {
	if (gnl_check_content("\n"))
		abort();
}

static void	test_tiny(void) {
	const char		*tests[] = {
		"empty file",
		"one char, no newline",
		"one char + newline",
		"lone newline"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(empty_file_test),
		!forked_test(one_char_no_nl_test),
		!forked_test(one_char_nl_test),
		!forked_test(lone_nl_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line tiny", num_tests, tests, passed);
}

/* ************************************************************************** */
/*                            basic multi-line                                */
/* ************************************************************************** */

static void	three_lines_test(void) {
	if (gnl_check_content("line1\nline2\nline3\n"))
		abort();
}

static void	no_trailing_nl_test(void) {
	if (gnl_check_content("line1\nline2\nline3"))
		abort();
}

static void	empty_lines_test(void) {
	if (gnl_check_content("\n\n\n"))
		abort();
}

static void	mixed_empty_lines_test(void) {
	if (gnl_check_content("a\n\nb\n\n\nc"))
		abort();
}

static void	spaces_and_tabs_test(void) {
	if (gnl_check_content("  spaced  \n\ttabbed\t\n   "))
		abort();
}

static void	test_basic(void) {
	const char		*tests[] = {
		"three lines + trailing newline",
		"three lines, no trailing newline",
		"only empty lines",
		"mixed empty and filled lines",
		"spaces and tabs preserved"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(three_lines_test),
		!forked_test(no_trailing_nl_test),
		!forked_test(empty_lines_test),
		!forked_test(mixed_empty_lines_test),
		!forked_test(spaces_and_tabs_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line basic", num_tests, tests, passed);
}

/* ************************************************************************** */
/*                              long lines                                    */
/* ************************************************************************** */

static char	*make_repeat(char c, size_t n, int trailing_nl) {
	char	*s = malloc(n + 2);
	size_t	i;

	if (!s)
		error_exit("gnl-fairy: malloc failed");
	memset(s, c, n);
	i = n;
	if (trailing_nl)
		s[i++] = '\n';
	s[i] = '\0';
	return s;
}

static void	long_line_no_nl_test(void) {
	char	*c = make_repeat('x', 5000, 0);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

static void	long_line_nl_test(void) {
	char	*c = make_repeat('y', 5000, 1);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

static void	giant_line_test(void) {
	char	*c = make_repeat('z', 20000, 1);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

static void	test_long(void) {
	const char		*tests[] = {
		"5000-char line, no newline",
		"5000-char line + newline",
		"20000-char giant line"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(long_line_no_nl_test),
		!forked_test(long_line_nl_test),
		!forked_test(giant_line_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line long lines", num_tests, tests, passed);
}

/* ************************************************************************** */
/*                              many lines                                    */
/* ************************************************************************** */

static void	many_short_lines_test(void) {
	const char	*unit = "hello\n";
	size_t		ulen = 6;
	size_t		count = 1000;
	char		*buf = malloc(ulen * count + 1);
	int			r;

	if (!buf)
		error_exit("gnl-fairy: malloc failed");
	for (size_t i = 0; i < count; i++)
		memcpy(buf + i * ulen, unit, ulen);
	buf[ulen * count] = '\0';
	r = gnl_check_content(buf);
	free(buf);
	if (r)
		abort();
}

static void	varied_length_lines_test(void) {
	if (gnl_check_content("a\nbb\nccc\ndddd\neeeee\nffffff\n"))
		abort();
}

static void	test_many(void) {
	const char		*tests[] = {
		"1000 identical short lines",
		"increasing length lines"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(many_short_lines_test),
		!forked_test(varied_length_lines_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line many lines", num_tests, tests, passed);
}

/* ************************************************************************** */
/*                          fd handling / stdin                               */
/* ************************************************************************** */

static void	negative_fd_test(void) {
	if (get_next_line(-1) != NULL)
		abort();
}

static void	closed_fd_test(void) {
	int	fd = open("/dev/null", O_RDONLY);

	close(fd);
	if (get_next_line(fd) != NULL)
		abort();
}

static void	dev_null_test(void) {
	int		fd = open("/dev/null", O_RDONLY);
	char	*got = get_next_line(fd);

	close(fd);
	if (got) {
		free(got);
		abort();
	}
}

static void	stdin_test(void) {
	char	path[32];
	int		fd = gnl_tmp_open("from\nstdin\n", path);
	char	*l1;
	char	*l2;
	char	*l3;
	int		ok;

	dup2(fd, STDIN_FILENO);
	close(fd);
	l1 = get_next_line(STDIN_FILENO);
	l2 = get_next_line(STDIN_FILENO);
	l3 = get_next_line(STDIN_FILENO);
	ok = l1 && !strcmp(l1, "from\n") && l2 && !strcmp(l2, "stdin\n") && !l3;
	free(l1);
	free(l2);
	free(l3);
	unlink(path);
	if (!ok)
		abort();
}

static void	test_fd(void) {
	const char		*tests[] = {
		"negative fd returns NULL",
		"closed fd returns NULL",
		"empty source (/dev/null) returns NULL",
		"reads from stdin (fd 0)"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(negative_fd_test),
		!forked_test(closed_fd_test),
		!forked_test(dev_null_test),
		!forked_test(stdin_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line fd handling", num_tests, tests, passed);
}

int	main(void) {
	test_tiny();
	test_basic();
	test_long();
	test_many();
	test_fd();
	return (g_tests_failed ? 1 : 0);
}
