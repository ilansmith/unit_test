#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

#include "unit_test.h"

/* unit_test.h should be included after the tested code's header files so that
 * if it defines any of the following definitions it should get precedence */

#ifdef CONFIG_KUT_COLOURS
#undef C_CYAN
#define C_CYAN "\033[01;36m"
#undef C_RED
#define C_RED "\033[01;31m"
#undef C_GREEN
#define C_GREEN "\033[01;32m"
#undef C_BLUE
#define C_BLUE "\033[01;34m"
#undef C_GREY
#define C_GREY "\033[00;37m"
#undef C_NORMAL
#define C_NORMAL "\033[00;00;00m"
#undef C_HIGHLIGHT
#define C_HIGHLIGHT "\033[01m"
#else
#undef C_CYAN
#define C_CYAN ""
#undef C_RED
#define C_RED ""
#undef C_GREEN
#define C_GREEN ""
#undef C_BLUE
#define C_BLUE ""
#undef C_GREY
#define C_GREY ""
#undef C_NORMAL
#define C_NORMAL ""
#undef C_HIGHLIGHT
#define C_HIGHLIGHT ""
#endif

#ifndef CURSOR_POS_SAVE
#define CURSOR_POS_SAVE "\033[s"
#endif
#ifndef CURSOR_POS_RESTORE
#define CURSOR_POS_RESTORE "\033[u"
#endif
#ifndef CURSOR_MOV_UP
#define CURSOR_MOV_UP "\033[%dA"
#endif
#ifndef CURSOR_MOV_DOWN
#define CURSOR_MOV_DOWN "\033[%dB"
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX_APP_NAME_SZ 256

#define UT_DISABLED(tests, t) ((tests)->is_disabled && \
	(tests)->is_disabled((t)->disabled))

typedef int (*vio_t)(const char *format, va_list ap);

#define UNIT_TEST_DECLERATIONS
#include "tests.h"

#define UNIT_TEST_ENTRIES
static struct unit_test *tests[] = {
#include "tests.h"
};

/* io functionality */
int vscanf(const char *format, va_list ap);
static int first_comment;

int ask_user;

static int vio_colour(vio_t vfunc, char *colour, char *fmt, va_list va)
{
	int ret;

	if (!colour)
		colour = C_NORMAL;

	ret = printf("%s", colour);
	ret += vfunc(fmt, va);
	ret += printf("%s", C_NORMAL);
	fflush(stdout);

	return ret;
}

static int p_colour(char *colour, char *fmt, ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = vio_colour(vprintf, colour, fmt, va);
	va_end(va);

	return ret;
}

static void p_test_summery(int total, int passed, int failed, int known_issues,
		int disabled, char *summery_comment)
{
	printf("\ntest summery%s%s%s\n", summery_comment ? " (" : "",
		summery_comment ? summery_comment : "", summery_comment ?
		")" : "");
	printf("------------\n");
	printf("%stotal:        %i%s\n", C_HIGHLIGHT, total, C_NORMAL);
	printf("passed:       %i\n", passed);
	printf("failed:       %i\n", failed);
	printf("known issues: %i\n", known_issues);
	printf("disabled:     %i\n", disabled);
}

static char *app_name(char *argv0)
{
	char *name, *ptr;
	static char path[MAX_APP_NAME_SZ];

	snprintf(path, MAX_APP_NAME_SZ, "%s", argv0);
	for (name = ptr = path; *ptr; ptr++) {
		if (*ptr != '/')
			continue;

		name = ptr + 1;
	}
	return name;
}

static void test_usage(char *path)
{
	char *app = app_name(path);

	printf("usage:\n"
		"%s               - run all tests\n"
		"  or\n"
		"%s <test>        - run a specific test\n"
		"  or\n"
		"%s <from> <to>   - run a range of tests\n"
		"  or\n"
		"%s list          - list all tests\n",
		app, app, app, app);
}

static int test_getarg(char *arg, int *arg_ival, int min, int max)
{
	char *err;

	*arg_ival = strtol(arg, &err, 10);
	if (*err)
		return -1;
	if (*arg_ival < min || *arg_ival > max) {
		printf("test number out of range: %i\n", *arg_ival);
		return -1;
	}
	return 0;
}

static int test_getargs(int argc, char *argv[], int *from, int *to, int max)
{
	if (argc > 3) {
		test_usage(argv[0]);
		return -1;
	}

	if (argc == 1) {
		*from = 0;
		*to = max;
		ask_user = 1;
		return 0;
	}

	/* 2 <= argc <= 3*/
	if (test_getarg(argv[1], from, 1, max)) {
		test_usage(argv[0]);
		return -1;
	}

	if (argc == 2) {
		*to = *from;
	} else { /* argc == 3 */
		if (test_getarg(argv[2], to, *from, max)) {
			test_usage(argv[0]);
			return -1;
		}
	}

	(*from)--; /* map test number to table index */
	return 0;
}

static void test_pre_post(int pre, void(*init)(int argc, char *argv[]),
	int argc, char *argv[])
{
	p_colour(C_HIGHLIGHT, "Tests %s\n", pre ? "Init" : "Uninit");
	init(argc, argv);
	if (pre)
		printf("\n");
}

static void test_init(void(*init)(int argc, char *argv[]), int argc,
	char *argv[])
{
	test_pre_post(1, init, argc, argv);
}

static void test_uninit(void(*init)(int argc, char *argv[]), int argc,
	char *argv[])
{
	test_pre_post(0, init, argc, argv);
}

static int is_list_tests(int argc, char *argv[], struct unit_test *ut)
{
	int i, count = ut->count;
	char *list_comment = ut->list_comment;
	struct single_test *tests = ut->tests;

	if (argc != 2 || strcmp(argv[1], "list"))
		return 0;

	p_colour(C_HIGHLIGHT, "%s unit tests%s%s%s\n", app_name(argv[0]),
		list_comment ? " (" : "", list_comment ? list_comment : "",
		list_comment ? ")" : "");
	for (i = 0; i < count - 1; i++) {
		struct single_test *t = &tests[i];
		int is_disabled = UT_DISABLED(ut, t);

		printf("%i. ", i + 1);
		p_colour(is_disabled ? C_GREY : C_NORMAL, "%s",
				t->description);
		if (is_disabled) {
			p_colour(C_CYAN, " (disabled)");
		} else if (t->known_issue) {
			p_colour(C_BLUE, " (known issue: ");
			p_colour(C_GREY, t->known_issue);
			p_colour(C_BLUE, ")");
		}
		printf("\n");
	}

	return 1;
}

static int unit_test(int argc, char *argv[], struct unit_test *ut)
{
	struct single_test *t;
	int from, to, max = ut->count, ret;
	int total = 0, disabled = 0, passed = 0, failed = 0, known_issues = 0;

	if (is_list_tests(argc, argv, ut))
		return 0;

	if (ut->tests_init)
		test_init(ut->tests_init, argc, argv);

	if (test_getargs(argc, argv, &from, &to, max - 1))
		return -1;

	for (t = &ut->tests[from]; t < ut->tests + MIN(to, max); t++) {
		first_comment = 1;
		total++;
		p_colour(C_HIGHLIGHT, "%i. %s ", from + total, t->description);
		if (!t->func) {
			p_colour(C_CYAN, "function does not exist\n");
			return -1;
		}
		printf("\n");
		fflush(stdout);

		if (UT_DISABLED(ut, t)) {
			disabled++;
			p_colour(C_CYAN, "disabled\n");
			continue;
		}

		if (t->known_issue) {
			p_colour(C_BLUE, "known issue: ");
			p_colour(C_NORMAL, "%s\n", t->known_issue);
			known_issues++;
			continue;
		}

		if (ut->pre_test)
			ut->pre_test();

		if ((ret = t->func())) {
			p_colour(C_RED, "Failed");
			failed++;
		} else {
			p_colour(C_GREEN, "OK");
			passed++;
		}

		if (ut->post_test)
			ut->post_test();

		printf("\n");
	}

	if (ut->tests_uninit)
		test_uninit(ut->tests_uninit, argc, argv);

	p_test_summery(total, passed, failed, known_issues, disabled,
		ut->summery_comment);
	return 0;
}

int main(int argc, char **argv)
{
	int i;

	for (i = 0; i < ARRAY_SZ(tests); i++)
		unit_test(argc, argv, tests[i]);

	return 0;
}

