#include <string>
#include "envVarTaskID.h"

using namespace std;

string getEnvVar(string const & key) {
    char * val = getenv(key.c_str());
    return val == NULL ? string("") : string(val);
}
