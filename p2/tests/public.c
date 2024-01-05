#include <arpa/inet.h>
#include <assert.h>
#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "../src/dhcp.c"

START_TEST (C_test_template)
{
  int x = 5;
  int y = 5;
  ck_assert_int_eq (x, y);
}
END_TEST

START_TEST (invalid_filename)
{
  pid_t pid = fork ();
  if (pid == 0)
    print_file ("bad.txt");
  int status;
  wait (&status);
  int return_value = WEXITSTATUS (status);
  ck_assert_int_eq (return_value, 1);
}
END_TEST

void public_tests (Suite *s)
{
  TCase *tc_public = tcase_create ("Public");
  tcase_add_test (tc_public, C_test_template);
  tcase_add_test (tc_public, invalid_filename);
  suite_add_tcase (s, tc_public);
}

