#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>

#include "unit_test.h"

/* unit_test.h should be included after the tested code's header files so that
 * if it defines any of the following definitions it should get precedence */

#ifdef CONFIG_UT_COLOURS
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
#undef C_ITALIC
#define C_ITALIC "\033[03m"
#undef C_UNDERLINE
#define C_UNDERLINE "\033[04m"
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
#undef C_UNDERLINE
#define C_UNDERLINE ""
#undef C_ITALIC
#define C_ITALIC ""
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

#define UT_DISABLED(tests, t) ({ \
	int __ret; \
	do { \
		if ((tests)->is_test_disabled) \
			__ret = (tests)->is_test_disabled((t)->disabled); \
		else \
			__ret = (t)->disabled; \
	} while (0); \
	__ret; \
})

#define LIST_TEST -1
#define RUN_ALL_TEST 0

typedef int (*vio_t)(const char *format, va_list ap);

struct unit_test_wrapper {
	struct unit_test *ut;
	unsigned long *map;
};

#define UNIT_TEST_DECLERATIONS
#include "tests.h"

#define UNIT_TEST_ENTRIES
static struct unit_test_wrapper all_tests[] = {
#include "tests.h"
};

/* io functionality */
int vscanf(const char *format, va_list ap);
static int first_comment;

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

static struct unit_test_wrapper *module2ut(char *module)
{
	int i;

	for (i = 0; i < ARRAY_SZ(all_tests) &&
		strcmp(module, all_tests[i].ut->module); i++);

	if (i == ARRAY_SZ(all_tests)) {
		printf("Error: '%s' is not a valid test modlue\n", module);
		return NULL;
	}

	return all_tests + i;
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

static char *app_name(char *arg)
{
	char *app_name;
	
	app_name = strrchr(arg, '/');
	if (app_name)
		app_name += 1;
	else
		app_name = arg;

	return app_name;
}

static void test_usage(char *path)
{
	char *app = app_name(path);

	printf(C_HIGHLIGHT "%s [OPTIONS]" C_NORMAL \
			"     - run kernel unit tests\n" \
		"\n" \
		"  Where OPTIONS:\n"
		"\n" \
		C_HIGHLIGHT \
		"-l, --list [module1 module2 ...]" \
		C_NORMAL "\n" \
		"    List available unit tests.\n" \
		"\n" \
		"    If no tests are specified then a summary of all " \
			"available tests is listed.\n" \
		"\n" \
		"    Specific module names may be selected to be listed in " \
			"which case they will be listed in more detail.\n"
		"    The available test names are those listed when using " \
			C_UNDERLINE "--list" C_NORMAL " without arguments.\n" \
		"\n" \
		C_HIGHLIGHT \
		"-t, --test <module> [#1 [#2 [...]]]" \
		C_NORMAL "\n"
		"    Specify a specific unit test to run.\n" \
		"\n" \
		"    " C_ITALIC "module" C_NORMAL " is given by using the " \
			C_UNDERLINE "--list" C_NORMAL " option without " \
			"arguments.\n" \
		"    If no test ids are provided, then all the tests of " \
			C_ITALIC "module" C_NORMAL " are run.\n" \
		"\n" \
		"    Execution can be limited to running a subset of the " \
			C_ITALIC "module" C_NORMAL "'s tests.\n"
		"    This is done by specific test numbers as are given by " \
			C_UNDERLINE "--list " C_ITALIC "module" C_NORMAL ".\n" \
		"\n" \
		C_HIGHLIGHT "-h, --help" C_NORMAL "\n" \
		"    Print this help message and exit.\n" \
		"\n" \
		C_HIGHLIGHT \
		"If no options are used, %s runs all the tests." \
		C_NORMAL "\n", app, app);
}

static int test_pre_post(int pre, struct unit_test *ut)
{
	int ret;
	int (*func)(void) = pre? ut->pre_all_tests: ut->post_all_tests;

	if (!func)
		return 0;

	if (!pre)
		printf("\n");

	p_colour(C_HIGHLIGHT, "%s %s\n", ut->description,
		pre ? "Init" : "Uninit");
	ret = func();

	if (ret) {
		printf(C_RED "%s failed\n" C_NORMAL,
			pre ? "pre_all_tests" : "post_all_tests");
	}
	return ret;
}

static void init_common(void)
{
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

static int pre_single_test(struct unit_test *ut)
{
	init_common();

	if (!ut->pre_single_test)
		return 0;

	return ut->pre_single_test();
}

static int post_single_test(struct unit_test *ut)
{
	if (!ut->post_single_test)
		return 0;

	return ut->post_single_test();
}

static int setup_map(struct unit_test_wrapper *wrapper)
{
	int size;

	if (wrapper->map)
		return 0;

	size = (wrapper->ut->count + sizeof(unsigned long) - 1) /
		sizeof(unsigned long);
	if (!(wrapper->map = calloc(size, sizeof(unsigned long))))
		return -1;

	return 0;
}

static void teardown_map(struct unit_test_wrapper *wrapper)
{
	free(wrapper->map);
	wrapper->map = NULL;
}

static void map_set_get(unsigned long **map, int *shift)
{
	int div = *shift/sizeof(unsigned long);

	*map += div;
	*shift -= div*sizeof(unsigned long);
}

static void map_set(unsigned long *map, int shift)
{
	map_set_get(&map, &shift);
	*map |= 1 << shift;
}

static int map_get(unsigned long *map, int shift)
{
	map_set_get(&map, &shift);
	return *map & 1 << shift ? 1 : 0;
}

static int unit_test(struct unit_test *ut, unsigned long *map)
{
	int total = 0, disabled = 0, passed = 0, failed = 0, known_issues = 0;
	int len, i;

	/* print unit test description as header */
	len = printf(C_HIGHLIGHT "%s Unit Tests\n", ut->description);
	for (i = 0; i < len - 6; i++)
		printf("-");
	printf("\n"  C_NORMAL);

	if (test_init(ut))
		return -1;

	for (i = 0; i < ut->count; i++) {
		struct single_test *t;

		if (map && !(map_get(map, i)))
			continue;

		t = ut->tests + i;
		first_comment = 1;
		total++;
		p_colour(C_HIGHLIGHT, "%i. %s ", i + 1, t->description);
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

		if (pre_single_test(ut)) {
			printf(C_RED "pre test failed, continuing...\n"
				C_NORMAL);
			continue;
		}

		if ((t->func())) {
			p_colour(C_RED, "Failed");
			failed++;
		} else {
			p_colour(C_GREEN, "OK");
			passed++;
		}

		if (post_single_test(ut)) {
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
	printf(C_HIGHLIGHT "%-5s %-20s %s\n", "num", "module", "description");
	printf(C_HIGHLIGHT "%-5s %-20s %s\n", "---", "------", "-----------");

	/* list tests */
	for (i = 0; i < ARRAY_SZ(all_tests); i++) {
		printf("%3.d.  %-20s %s\n", i+1, all_tests[i].ut->module,
			all_tests[i].ut->description);
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
		int is_test_disabled = UT_DISABLED(ut, t);

		printf("%i. ", i + 1);
		p_colour(is_test_disabled ? C_GREY : C_NORMAL, "%s",
				t->description);
		if (is_test_disabled) {
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
		{
			.name = "help",
			.val = 'h',
			.has_arg = no_argument,
			.flag = NULL,
		},
		{ 0 }
	};
	enum {
		arg_none,
		arg_list,
		arg_test,
		arg_help,
	} argtype = arg_none;
	struct unit_test_wrapper *wrapper;

	while ((arg = getopt_long(argc, argv, optstring, longopts, NULL)) !=
		-1) {
		switch(arg) {
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
				wrapper = module2ut(argv[i]);
				if (!wrapper) {
					printf("Error: unknown unit test - "
						"%s\n", argv[i]);
					continue;
				}

				list_tests(wrapper->ut);
			}
			break;

		case 't':
			if (argtype != arg_none && argtype != arg_test)
				goto err_arg;
			if (argtype == arg_test)
				printf("\n");
			else
				argtype = arg_test;

			/* handle a specified unit test */
			if (!(wrapper = module2ut(optarg))) {
				printf("Error: unknown unit test - %s\n",
					optarg);
				break;
			}

			/* run a specified selection of its tests */
			while (optind < argc) {
				int test;
				char *err;

				test = strtol(argv[optind], &err, 10);
				if (!*err) {
					optind++;

					if (!test ||
						test > wrapper->ut->count) {
						printf("Error: %s out of " \
							"range - %d\n", optarg,
							test);
						continue;
					}

					if (setup_map(wrapper)) {
						printf("Error: out of " \
							"memory\n");
						return -1;
					}

					map_set(wrapper->map, test - 1);
				} else {
					if (strcmp(argv[optind], "-t") &&
						strncmp(argv[optind], "--test",
						MAX(strlen(argv[optind]), 3))) {
						printf("Error: not a test " \
							"number: '%s'\n",
							argv[optind]);
					}
					break;
				}
			}

			unit_test(wrapper->ut, wrapper->map);
			teardown_map(wrapper);
			break;

		case 'h':
			if (argtype != arg_none)
				goto err_arg;
			argtype = arg_help;

			test_usage(argv[0]);
			return 0;

		default:
			test_usage(argv[0]);
			return -1;
		}
	}

	/* run all tests */
	if (argc == 1) {
		for (i = 0; i < ARRAY_SZ(all_tests); i++) {
			if (i)
				printf("\n");
			unit_test(all_tests[i].ut, 0);
		}
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

