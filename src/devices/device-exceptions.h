
#include	<cstdio>
#include <stdexcept>

#define MAX_MESSAGE_SIZE 256

class std_exception_string : public std::exception
{
private:
  char text[MAX_MESSAGE_SIZE];
public:
  std_exception_string(const std::string & s)
  {
    snprintf(text, MAX_MESSAGE_SIZE, "%s ", s.c_str());
  }

  [[nodiscard]] const char * what() const noexcept override
  {
    return text;
  }
};

