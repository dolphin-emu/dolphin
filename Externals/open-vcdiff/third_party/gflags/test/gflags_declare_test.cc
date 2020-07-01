#include <gflags/gflags.h>

DEFINE_string(message, "", "The message to print");
void print_message(); // in gflags_declare_flags.cc

int main(int argc, char **argv)
{
  gflags::SetUsageMessage("Test compilation and use of gflags_declare.h");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  print_message();
  return 0;
}
