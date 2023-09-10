#include <sys/socket.h>
#include "address.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;

void get_URL(const string &host, const string &path) {
  // You will need to connect to the "http" service on
  // the computer whose name is in the "host" string,
  // then request the URL path given in the "path" string.

  // Then you'll need to print out everything the server sends back,
  // (not just one call to read() -- everything) until you reach
  // the "eof" (end of file).

  TCPSocket tcp_socket;
  tcp_socket.connect(Address(host, "http"));
  std::string msg;
  msg += "GET " + path + " HTTP/1.1\r\n";
  msg += "Host: " + host + "\r\n";
  msg += "Connect: close\r\n";
  msg += "\r\n";
  tcp_socket.write(msg);

  while (!tcp_socket.eof()) {
    std::string tmp_msg;
    tcp_socket.read(tmp_msg);
    std::cout << tmp_msg;
  }

  tcp_socket.close();
}

int main(int argc, char *argv[]) {
  try {
    if (argc <= 0) {
      abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
    }
    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if (argc != 3) {
      cerr << "Usage: " << argv[0] << " HOST PATH\n";
      cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }
    // Get the command-line arguments.
    const string host = argv[1];
    const string path = argv[2];
    // Call the student-written function.
    get_URL(host, path);
  } catch (const exception &e) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
