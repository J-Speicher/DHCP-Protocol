#include <stdlib.h>

#include "dhcp.h"
#include "format.h"

int
main (int argc, char **argv)
{
  read_file (argv[1]);
  return EXIT_SUCCESS;
}
