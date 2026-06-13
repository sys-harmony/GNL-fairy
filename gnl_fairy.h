#ifndef GNL_FAIRY_H
# define GNL_FAIRY_H

// =============================
// 📚 Libraries
// =============================
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
# include <fcntl.h>
# include <stddef.h>

// =============================
// 🎛️  BUFFER_SIZE fallback
// =============================
// The real value is always injected by run.sh through -D BUFFER_SIZE=<n>.
// This fallback only lets the harness compile on its own (cosmetic).
# ifndef BUFFER_SIZE
#  define BUFFER_SIZE 42
# endif

// =============================
// 🎨 Global define(s)
// =============================
# define GREEN			"\033[0;32m"
# define RED			"\033[0;31m"
# define RESET			"\033[0m"

// Per-test wall-clock guard (seconds): a broken get_next_line can loop
// forever, so every forked test arms an alarm to fail instead of hanging.
# define GNL_TIMEOUT	10

// =============================
// 🌍 Global variable(s)
// =============================
extern int	g_malloc_wrap_enabled;
extern int	g_malloc_count;
extern int	g_malloc_fail_at;
extern int	g_read_error_enabled;
extern int	g_tests_failed;

// =============================
// 🪄 Macro(s)
// =============================
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

// =============================
// 📋 Function prototype(s)
// =============================
void		*__real_malloc(size_t size);
void		*__wrap_malloc(size_t size);
ssize_t		__real_read(int fd, void *buf, size_t count);
ssize_t		__wrap_read(int fd, void *buf, size_t count);
int			all_tests_passed(const int *passed, const size_t num_tests);
void		print_test_results(char *function_name, const size_t num_tests,
				const char *tests[], const int passed[]);
void		error_exit(char *msg);
int			forked_test(void (*test_func)(void));

// GNL-specific helpers (defined in utils.c)
int			gnl_tmp_open(const char *content, char *path);
char		*gnl_expected_next(const char **cursor);
int			gnl_check_content(const char *content);

// The function under test. Its name and signature are fixed by the subject,
// so we declare it here directly: the harness never needs to #include the
// student header (which may sit anywhere / carry guards we don't want).
char		*get_next_line(int fd);

#endif //GNL_FAIRY_H
