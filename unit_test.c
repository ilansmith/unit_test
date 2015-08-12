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

#define LIST_TEST -1

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

static struct unit_test *name2ut(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SZ(tests) && strcmp(name, tests[i]->name); i++);

	if (i == ARRAY_SZ(tests)) {
		printf("Error: '%s' is not a valid test name\n", name);
		return NULL;
	}

	return tests[i];
}

static void p_test_summery(char *description, int total, int passed, int failed,
	int known_issues, int disabled, char *summery_comment)
{
	int i, len;

	len = printf("\n%s Test Summery%s%s%s\n", description,
		summery_comment ? " (" : "",
		summery_comment ? summery_comment : "", summery_comment ?
		")" : "");
	for (i = 0; i < len - 2; i++)
		printf("-");
	printf("\n");

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

static void test_pre_post(int pre, char *description, void(*init)(void))
{
	p_colour(C_HIGHLIGHT, "%s %s\n", description, pre ? "Init" : "Uninit");
	init();
	if (pre)
		printf("\n");
}

static void test_init(char *description, void(*init)(void))
{
	test_pre_post(1, description, init);
}

static void test_uninit(char *description, void(*init)(void))
{
	test_pre_post(0, description, init);
}

static int list_test(struct unit_test *ut)
{
	int i, count = ut->count;
	char *list_comment = ut->list_comment;
	struct single_test *tests = ut->tests;

	p_colour(C_HIGHLIGHT, "%s Unit Tests%s%s%s\n", ut->description,
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

static int unit_test(struct unit_test *ut, int action)
{
	struct single_test *t;
	int from, to, ret;
	int total = 0, disabled = 0, passed = 0, failed = 0, known_issues = 0;
	int len, i;

	if (action == LIST_TEST) {
		list_test(ut);
		return 0;
	}

	if (!action) {
		from = 0;
		to = ut->count - 1;
	} else {
		from = action - 1;
		to = action;
	}

	/* print unit test description as header */
	len = printf(C_HIGHLIGHT "%s Unit Tests\n", ut->description);
	for (i = 0; i < len - 6; i++)
		printf("-");
	printf("\n"  C_NORMAL);

	if (ut->tests_init)
		test_init(ut->description, ut->tests_init);

	for (t = &ut->tests[from]; t < ut->tests + to; t++) {
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
		test_uninit(ut->description, ut->tests_uninit);

	p_test_summery(ut->description, total, passed, failed, known_issues,
		disabled, ut->summery_comment);
	return 0;
}

static void do_list_tests(void)
{
	int i;

	/* print header */
	printf(C_HIGHLIGHT "%-5s %-30s %s\n", "num", "description",
			"name");
	for (i = 0; i < 45; i++)
		printf("-");
	printf("\n" C_NORMAL);

	/* list tests */
	for (i = 0; i < ARRAY_SZ(tests); i++) {
		printf("%3.d.  %-30s %s\n", i+1, tests[i]->description,
				tests[i]->name);
	}
}

int main(int argc, char **argv)
{
	int i;

	/* run all tests */
	if (argc == 1) {
		for (i = 0; i < ARRAY_SZ(tests); i++)
			unit_test(tests[i], 0);
		return 0;
	}

	/* help */
	if (!strcmp(argv[1], "help")) {
		test_usage(argv[0]);
		return 0;
	}

	/* list tests */
	if (!strcmp(argv[1], "list")) {
		/* list all tests */
		if (argc == 2) {
			do_list_tests();
			return 0;
		}

		/* list specific tests */
		for (i = 2; i < argc; i++) {
			struct unit_test *ut = name2ut(argv[i]);

			if (!ut)
				continue;

			unit_test(ut, LIST_TEST);
		}

		return 0;
	}

	/* run tests */
	for (i = 1; i < argc; i++) {
		struct unit_test *ut = name2ut(argv[i]);
		int action = 0;

		if (!ut)
			continue;

		/* possibly run a specific test */
		if (i < argc) {
			int next_arg;
			char *err;

			next_arg = strtol(argv[i + 1], &err, 10);
			if (!*err) {
				action = next_arg;
				i++;

				if (!action || action > ut->count) {
					printf("Error: %s out of range - %d\n",
							argv[i], action);
					continue;
				}
			}
		}

		unit_test(ut, action);
	}

	return 0;
}

