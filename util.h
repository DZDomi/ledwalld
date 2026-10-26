#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "models/request.pb.h"

using namespace std;
using namespace boost::posix_time;

namespace util {
    void log(string type, string text);
}
