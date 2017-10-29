/* add tests here */

#if defined(UNIT_TEST_DECLERATIONS)
#define UNIT_TEST(test) extern struct unit_test test;
#elif defined(UNIT_TEST_ENTRIES)
#define UNIT_TEST(test) \
	{ \
		.ut = &test, \
		.map = NULL, \
	},
#endif

/* Unit Tests */
#ifdef CONFIG_SLW
UNIT_TEST(ut_slw)
#endif

#undef UNIT_TEST_DECLERATIONS
#undef UNIT_TEST_ENTRIES
#undef UNIT_TEST

