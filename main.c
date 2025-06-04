#include "spdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  srand(time(NULL));

  spdf_t *doc = create_spdf(32);
  if (!doc) {
    fprintf(stderr, "Failed to create spdf\n");
    return EXIT_FAILURE;
  }
  sleep(1);

  for (size_t i = 0; i < doc->max_streams - 2; i++) {
    if (!add_stream(create_stream((void *)i, 1), doc)) {
      destroy_spdf(doc);
      fprintf(stderr, "failed to add stream\n");
      return EXIT_FAILURE;
    }
  }

  add_stream(create_stream((void *)0xdeadbeef, 1), doc);
  print_spdf(doc);

  sleep(1);
  for (size_t i = 2; i < doc->max_streams; i++) {
    if (!remove_stream(doc->streams[i], doc)) {
      destroy_spdf(doc);
      fprintf(stderr, "failed to remove stream\n");
      return EXIT_FAILURE;
    }
  }

  remove_stream(create_stream((void *)0xdeadbeef, 1), doc);
  print_spdf(doc);

  if (!destroy_spdf(doc)) {
    fprintf(stderr, "failed to destroy spdf\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
