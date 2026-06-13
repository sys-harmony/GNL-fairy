#include "gnl_fairy.h"

#ifndef VERBOSE
# define VERBOSE 0
#endif

/*
 * Bonus = managing several file descriptors at once with a single static
 * variable: reading from fd A then fd B then A again must not lose A's state.
 * "Only one static variable" cannot be observed from the outside (it is a
 * code/norm matter), so we validate the behavioural half: per-fd independence.
 */

#define NB_FD 3

static int			g_fd[NB_FD];
static const char	*g_cur[NB_FD];
static char			g_path[NB_FD][32];
static const char	*g_content[NB_FD] = {
	"A1\nA2\nA3\n",
	"B1\nB2\n",
	"C1\nC2\nC3\nC4\n"
};

static void	open_all(void) {
	for (int i = 0; i < NB_FD; i++) {
		g_fd[i] = gnl_tmp_open(g_content[i], g_path[i]);
		g_cur[i] = g_content[i];
	}
}

static void	close_all(void) {
	for (int i = 0; i < NB_FD; i++) {
		close(g_fd[i]);
		unlink(g_path[i]);
	}
}

/* Reads one line from fd #i and checks it matches that fd's own expectation. */
static int	step(int i) {
	char	*expected = gnl_expected_next(&g_cur[i]);
	char	*got = get_next_line(g_fd[i]);
	int		ok;

	if (expected == NULL)
		ok = (got == NULL);
	else
		ok = (got && strcmp(got, expected) == 0);
	free(got);
	free(expected);
	return ok;
}

static void	interleaved_test(void) {
	int	ok = 1;

	open_all();
	ok &= step(0); ok &= step(1); ok &= step(2);
	ok &= step(0); ok &= step(1); ok &= step(2);
	ok &= step(0); ok &= step(2); ok &= step(1);
	ok &= step(2);
	/* every fd is now at EOF: each must yield NULL independently */
	ok &= step(0);
	ok &= step(1);
	ok &= step(2);
	close_all();
	if (!ok)
		abort();
}

static void	round_robin_test(void) {
	int	ok = 1;

	open_all();
	/* fully drain the three fds in strict rotation */
	for (int round = 0; round < 5; round++)
		for (int i = 0; i < NB_FD; i++)
			ok &= step(i);
	close_all();
	if (!ok)
		abort();
}

static void	two_fd_same_file_test(void) {
	char	pa[32];
	char	pb[32];
	int		fda = gnl_tmp_open("one\ntwo\nthree\n", pa);
	int		fdb = gnl_tmp_open("one\ntwo\nthree\n", pb);
	char	*a1;
	char	*b1;
	char	*a2;
	int		ok;

	a1 = get_next_line(fda);
	b1 = get_next_line(fdb);
	a2 = get_next_line(fda);
	ok = a1 && !strcmp(a1, "one\n") && b1 && !strcmp(b1, "one\n")
		&& a2 && !strcmp(a2, "two\n");
	free(a1);
	free(b1);
	free(a2);
	close(fda);
	close(fdb);
	unlink(pa);
	unlink(pb);
	if (!ok)
		abort();
}

static void	test_multi_fd(void) {
	const char		*tests[] = {
		"interleaved reads keep per-fd state",
		"round-robin full drain",
		"two fds on identical content stay independent"
	};
	const size_t	num_tests = ARRAY_SIZE(tests);
	const int		passed[] = {
		!forked_test(interleaved_test),
		!forked_test(round_robin_test),
		!forked_test(two_fd_same_file_test)
	};

	if (!all_tests_passed(passed, num_tests) || VERBOSE)
		print_test_results("get_next_line multi-fd (bonus)", num_tests, tests, passed);
}

int	main(void) {
	test_multi_fd();
	return (g_tests_failed ? 1 : 0);
}
