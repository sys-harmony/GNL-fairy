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
// 🎨 Global define(s)
// =============================
# define GREEN			"\033[0;32m"
# define RED			"\033[0;31m"
# define RESET			"\033[0m"
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
int			gnl_tmp_open(const char *content, char *path);
char		*gnl_expected_next(const char **cursor);
int			gnl_check_content(const char *content);
char		*get_next_line(int fd);

#endif //GNL_FAIRY_H
