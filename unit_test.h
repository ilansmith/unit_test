#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_

#ifndef ARRAY_SZ
#define ARRAY_SZ(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

struct single_test {
	char *description;
	char *known_issue;
	int (* func)(void);
	int disabled;
};

struct unit_test {
	char *name;
	char *description;
	struct single_test *tests;
	int count;
	char *list_comment;
	char *summery_comment;
	void (*tests_init)(void);
	void (*tests_uninit)(void);
	int (*is_disabled)(int flags);
	void (*pre_test)(void);
	void (*post_test)(void);
};

extern int ask_user;
#endif

