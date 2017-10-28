#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_

#ifndef ARRAY_SZ
#define ARRAY_SZ(arr) (sizeof(arr) ? sizeof(arr) / sizeof(arr[0]) : 0)
#endif

/** An single unit test entry type
 *
 * The unit test engine is essentially provided with an array of such entries
 * upon which it proceeds to plough through them and execute them one by one.
 *
 * @param description      A simple test description
 * @param known_issue      State known issue if exists - the test won't run
 * @param func             A pointer to a function implementing the test
 *                         Must return 0 on success, -1 otherwise
 * @param disable          Compile time test disabling
 *                         Can be used as either a boolean value, or as bit
 *                         field which is to be parsed by the is_test_disabled
 *                         callback when it exists in the module's
 *                         struct unit_test.
 */
struct single_test {
	char *description;
	char *known_issue;
	int (*func)(void);
	unsigned long disabled;
};

/** The input object provided to the unit test engine
 *
 * Every tested module must define a single such object and register it in
 * tests.h
 *
 * For example:
 * In my_module_tests.c we define:
 *
 *         struct unit_test my_tests = {
 *                 ...
 *         };
 *
 * Then in tests.h - in the designated area - we register it by adding:
 *
 *         UNIT_TEST(my_tests)
 *
 * @param module           A module name (must not contain white spaces)
 * @param description      A header for test run / module listing description
 * @param tests            An array of tests to be executed by the engine
 * @param count            The length of the tests array
 * @param list_comment     Optional addition to header of module test listing
 * @param summery_comment  Optional addition to the default test summery header 
 * @param pre_all_tests    A routine to be run prior to all tests in the module
 * @param post_all_tests   A routine to be run after all tests in the module
 * @param is_test_disabled A routine to run on disabled tests
 * @param pre_single_test  A routine to run before every test
 * @param post_single_test A routine to run after every test
 */
struct unit_test {
	char *module;
	char *description;
	struct single_test *tests;
	int count;
	char *list_comment;
	char *summery_comment;
	int (*pre_all_tests)(void);
	int (*post_all_tests)(void);
	int (*is_test_disabled)(unsigned long flags);
	int (*pre_single_test)(void);
	int (*post_single_test)(void);
};

#endif

