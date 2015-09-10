#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>

#include <linux/kut_mmzone.h>
#include <linux/kut_bug.h>
#include <linux/mmc/kut_host.h>

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
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MAX_APP_NAME_SZ 256

#define UT_DISABLED(tests, t) ((tests)->is_disabled && \
	(tests)->is_disabled((t)->disabled))

#define LIST_TEST -1
#define RUN_ALL_TEST 0

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

static int test_pre_post(int pre, struct unit_test *ut)
{
	int ret;
	int (*func)(void) = pre? ut->tests_init : ut->tests_uninit;

	if (!func)
		return 0;

	if (!pre)
		printf("\n");

	p_colour(C_HIGHLIGHT, "%s %s\n", ut->description,
		pre ? "Init" : "Uninit");
	ret = func();

	if (ret) {
		printf(C_RED "%s failed\n" C_NORMAL,
			pre ? "tests_init" : "tests_uninit");
	}
	return ret;
}

static void init_common(void)
{
	kut_bug_on_do_exit_set(false);
	kut_mem_pressure_set(KUT_MEM_AVERAGE);
	kut_host_index_reset();
	srandom(0);
}

static int test_init(struct unit_test *ut)
{
	init_common();

	return test_pre_post(1, ut);
}

static int test_uninit(struct unit_test *ut)
{
	return test_pre_post(0, ut);
}

static int pre_test(struct unit_test *ut)
{
	init_common();

	if (!ut->pre_test)
		return 0;

	return ut->pre_test();
}

static int post_test(struct unit_test *ut)
{
	if (!ut->post_test)
		return 0;

	return ut->post_test();
}
static int unit_test(struct unit_test *ut, int action)
{
	struct single_test *t;
	int from, to, ret;
	int total = 0, disabled = 0, passed = 0, failed = 0, known_issues = 0;
	int len, i;

	if (action == RUN_ALL_TEST) {
		from = 0;
		to = ut->count;
	} else {
		from = action - 1;
		to = action;
	}

	/* print unit test description as header */
	len = printf(C_HIGHLIGHT "%s Unit Tests\n", ut->description);
	for (i = 0; i < len - 6; i++)
		printf("-");
	printf("\n"  C_NORMAL);

	if (test_init(ut))
		return -1;

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

		if (pre_test(ut)) {
			printf(C_RED "pre test failed, continuing...\n"
				C_NORMAL);
			continue;
		}

		if ((ret = t->func())) {
			p_colour(C_RED, "Failed");
			failed++;
		} else {
			p_colour(C_GREEN, "OK");
			passed++;
		}

		if (post_test(ut)) {
			printf(C_RED "post test failed, continuing...\n"
				C_NORMAL);
			continue;
		}

		printf("\n");
	}

	if (test_uninit(ut))
		return -1;

	p_test_summery(ut->description, total, passed, failed, known_issues,
		disabled, ut->summery_comment);
	return 0;
}

static void list_tests_all(void)
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

static void list_tests_single(struct unit_test *ut)
{
	int i, count = ut->count;
	char *list_comment = ut->list_comment;
	struct single_test *tests = ut->tests;

	p_colour(C_HIGHLIGHT, "%s Unit Tests%s%s%s\n", ut->description,
		list_comment ? " (" : "", list_comment ? list_comment : "",
		list_comment ? ")" : "");
	for (i = 0; i < count; i++) {
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
}

static void list_tests(struct unit_test *ut)
{
	if (!ut)
		list_tests_all();
	else
		list_tests_single(ut);
}

int main(int argc, char **argv)
{
	int i, arg;
	char *optstring = "hlt:";
	struct option longopts[] = {
		{
			.name = "help",
			.val = 'h',
			.has_arg = no_argument,
			.flag = NULL,
		},
		{
			.name = "list",
			.val = 'l',
			.has_arg = no_argument,
			.flag = NULL,
		},
		{
			.name = "test",
			.val = 't',
			.has_arg = required_argument,
			.flag = NULL,
		},
		{ 0 }
	};
	enum {
		arg_none,
		arg_help,
		arg_list,
		arg_test,
	} argtype = arg_none;
	struct unit_test *ut;

	while ((arg = getopt_long(argc, argv, optstring, longopts, NULL)) !=
		-1) {
		switch(arg) {
		case 'h':
			if (argtype != arg_none)
				goto err_arg;
			argtype = arg_help;

			test_usage(argv[0]);
			return 0;
		case 'l':
			if (argtype != arg_none)
				goto err_arg;
			argtype = arg_list;

			/* list all tests */
			if (argc == 2) {
				list_tests(NULL);
				break;
			}

			/* list specific tests */
			for (i = 2; i < argc; i++) {
				ut = name2ut(argv[i]);
				if (!ut) {
					printf("Error: unknown unit test - "
						"%s\n", argv[i]);
					continue;
				}

				list_tests(ut);
			}
			break;
		case 't':
			if (argtype != arg_none && argtype != arg_test)
				goto err_arg;
			argtype = arg_test;

			/* handle a specified unit test */
			if (!(ut = name2ut(optarg))) {
				printf("Error: unknown unit test - %s\n",
					optarg);
				break;
			}

			/* run all its tests */
			if (optind == argc || !strcmp(argv[optind], "-t") ||
				!strncmp(argv[optind], "--test",
					MAX(strlen(argv[optind]), 3))) {
				unit_test(ut, RUN_ALL_TEST);
				break;
			}

			/* run a specified selection of its tests */
			while (optind < argc) {
				int test;
				char *err;

				test = strtol(argv[optind], &err, 10);
				if (!*err) {
					optind++;

					if (!test || test > ut->count) {
						printf("Error: %s out of " \
							"range - %d\n", optarg,
							test);
						continue;
					}
				} else {
					printf("Error: not a test number: " \
						"'%s'\n", argv[optind]);
					break;
				}

				unit_test(ut, test);
			}
			break;
		default:
			test_usage(argv[0]);
			return -1;
		}
	}

	/* run all tests */
	if (argc == 1) {
		for (i = 0; i < ARRAY_SZ(tests); i++)
			unit_test(tests[i], 0);
		return 0;
	} else if (argtype == arg_none) {
		printf("Error: unknown arguments - ");
		for (i = 1; i < argc ;i++)
			printf("%s ", argv[i]);
		printf("\n");
		return -1;
	}

	return 0;

err_arg:
	printf("Error: incompatible argument combination\n");
	return -1;

}

