#include "spdf.hpp"
#include <vector>

int main() {
  SPDF document;

  document.addStream(
      "UTF-8", "text/plain", "None", {0.0, 0.0},
      {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'});

  document.addStream("Base64", "application/octet-stream", "None", {0.0, 0.0},
                     {'S', 'G', 'V', 's', 'b', 'G', '8', 'g', 'V', '2', '9',
                      'y', 'b', 'G', 'Q', 'h'});

  document.print();

  return 0;
}
