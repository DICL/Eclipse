#include "application.hh"


Application::Application() : Node() { }
Application::Application(int f) : Node() { set_fd (f); }

bool Application::operator== (Application& that) {
  return (addr == that.addr);
}
