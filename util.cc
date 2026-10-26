#include "util.h"

namespace util {
    void log(string type, string text) {
        string now = to_iso_extended_string(microsec_clock::universal_time());
        cout << "[" << now << "] " << type << ": " << text << endl;
    }
}
