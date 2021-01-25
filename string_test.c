#include<stdio.h>
#include"siska_string.h"

int main()
{
	char* s0 = "hello world\n";
	char* s1 = "hello world\n";
	char* s2 = "hello\n";
	char* s3 = "hello";

	char buf[8];

	printf("strcmp(s0, s1): %d\n", siska_strcmp(s0, s1));
	printf("strcmp(s0, s2): %d\n", siska_strcmp(s0, s2));
	printf("strcmp(s0, s2, 5): %d\n", siska_strncmp(s0, s2, 5));
	printf("strcmp(s0, s2, 6): %d\n", siska_strncmp(s0, s2, 6));

	printf("strlen(s2): %d\n", siska_strlen(s2));
	printf("strlen(s3): %d\n", siska_strlen(s3));

	char* p = siska_strncpy(buf, s3, 8);
	printf("buf: %s, p - buf: %d\n", buf, (int)(p - buf));

	p = siska_strncpy(buf, s3, 4);
	buf[p - buf] = '\0';
	printf("buf: %s, p - buf: %d\n", buf, (int)(p - buf));
}
