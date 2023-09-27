// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

#ifndef LIBAV_UTILS_H
#define LIBAV_UTILS_H
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <uv.h>
#include <sstream>
#include <chrono>
#include <uuid/uuid.h>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <ios>
#include <fstream>
#include <stdlib.h>
using namespace std;

namespace app
{
string getFileName()
{
    stringstream ss;
    ss << time(nullptr) << ".ts";
    return ss.str();
}

static void pgm_save(unsigned char * buf, int wrap, int xsize, int ysize, char * filename)
{
    FILE * f;
    int i;

    f = fopen(filename, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

/**
 * Get now date RFC5322
 * @return string  Sun, 27 Mar 2022 23:20:22 +0500
 */
string getDateRFC5322()
{
    time_t current = time(nullptr);
    std::tm tm = *std::localtime(&current);
    stringstream ss;
    ss.imbue(std::locale("en_US.UTF-8"));
    ss << put_time(&tm, "%a, %d %b %Y %T %z");
    return ss.str();
}
/**
 * Make uuid
 * @return string
 */
string uuid()
{
    uuid_t out;
    uuid_generate(out);
    char uuid_str[37];
    uuid_unparse_lower(out, uuid_str);
    stringstream ss;
    ss << uuid_str;
    return ss.str();
}

string getISOCurrentTimestamp()
{
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    stringstream ss;
    ss << std::put_time(std::localtime(&t), "%FT%T%z");
    return ss.str();
}


void process_mem_usage(double & vm_usage, double & resident_set)
{
    using std::ifstream;
    using std::ios_base;
    using std::string;

    vm_usage = 0.0;
    resident_set = 0.0;

    // 'file' stat seems to give the most reliable results
    //
    ifstream stat_stream("/proc/self/stat", ios_base::in);

    // dummy vars for leading entries in stat that we don't care about
    //
    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;

    // the two fields we want
    //
    unsigned long vsize;
    long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
        >> utime >> stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >> starttime >> vsize
        >> rss; // don't care about the rest

    stat_stream.close();

    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
    vm_usage = vsize / 1024.0;
    resident_set = rss * page_size_kb;
}


void print_mem_usage()
{
    double a, b;
    process_mem_usage(a, b);
    cout << "vm_usage:" << a << " resident_set: " << b << endl;
}

void print_libuv_info()
{
    cout << "Libuv version:" << uv_version_string() << endl;
}
}

int getenv(const char * name, char * buffer, int buffer_size)
{
    if (name == nullptr || buffer_size < 0 || (buffer == nullptr && buffer_size > 0))
    {
        return INT_MIN;
    }

    int result = 0;
    int term_zero_idx = 0;
    size_t value_length = 0;

    const char * value = ::getenv(name);
    value_length = value == nullptr ? 0 : strlen(value);


    if (value_length > INT_MAX)
    {
        result = INT_MIN;
    }
    else
    {
        int int_value_length = static_cast<int>(value_length);
        if (int_value_length >= buffer_size)
        {
            result = -int_value_length;
        }
        else
        {
            term_zero_idx = int_value_length;
            result = int_value_length;
        }
    }

    if (buffer != nullptr)
    {
        buffer[term_zero_idx] = '\0';
    }
    return result;
}

inline std::string getenv_str(const char * s, const std::string & def)
{
    char buf[1024];
    int ret = getenv(s, buf, sizeof(buf));
    if (ret > 0)
    {
        return buf;
    }
    return def;
}
#endif // LIBAV_UTILS_H
