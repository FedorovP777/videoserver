
#include <uv.h>
#include <iostream>
#include <cassert>
#include <sstream>
#include "third-party/base64.h"
#include "SSLHmac.h"
using namespace std;
int main(int /*argc*/, char ** /*argv*/)
{
    for (int i = 0; i < 1000; i++)
    {
        stringstream ss;
        ss << "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGH" << i
           << endl;
        string b64_string = base64_encode(ss.str());
        assert(base64_decode(b64_string) == ss.str());
    }

    for (int i = 0; i < 1000; i++)
    {
        string key = "1234ABC";
        string signature = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRST";
        auto ssl_hmac = SSLHmac(key);
        ssl_hmac.update(signature);

        string result = ssl_hmac.get_str_result();
        assert(result == "cc66a1d9d78b1366365219e2e9d984ea39b073d8");
    }
    return 0;
}