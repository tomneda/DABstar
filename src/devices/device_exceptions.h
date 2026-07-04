
#include <string>
#include <stdexcept>

class std_exception_string : public std::exception
{
public:
  explicit std_exception_string(const std::string & s)
  {
    mText = s;
  }

  [[nodiscard]] const char * what() const noexcept override
  {
    return mText.c_str();
  }

private:
  std::string mText;
};

