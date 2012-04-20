#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "errors.hpp"

/* Note that repli_timestamp_t does NOT represent an actual timestamp; instead it's an arbitrary
counter. */

// for safety
class repli_timestamp_t {
public:
    uint32_t time;

    bool operator==(repli_timestamp_t t) const { return time == t.time; }
    bool operator!=(repli_timestamp_t t) const { return time != t.time; }
    bool operator<(repli_timestamp_t t) const { return time < t.time; }
    bool operator>(repli_timestamp_t t) const { return time > t.time; }
    bool operator<=(repli_timestamp_t t) const { return time <= t.time; }
    bool operator>=(repli_timestamp_t t) const { return time >= t.time; }

    repli_timestamp_t next() const {
        repli_timestamp_t t;
        t.time = time + 1;
        return t;
    }

    static const repli_timestamp_t distant_past;
    static const repli_timestamp_t invalid;
};

class write_message_t;
write_message_t &operator<<(write_message_t &msg, repli_timestamp_t tstamp);

class read_stream_t;
int deserialize(read_stream_t *s, repli_timestamp_t *tstamp);


struct const_charslice {
    const char *beg, *end;
    const_charslice(const char *beg_, const char *end_) : beg(beg_), end(end_) { }
    const_charslice() : beg(NULL), end(NULL) { }
};

typedef uint64_t microtime_t;

microtime_t current_microtime();

/* General exception to be thrown when some process is interrupted. It's in
`utils.hpp` because I can't think where else to put it */
class interrupted_exc_t : public std::exception {
public:
    const char *what() const throw () {
        return "interrupted";
    }
};

// Like std::max, except it's technically not associative.
repli_timestamp_t repli_max(repli_timestamp_t x, repli_timestamp_t y);


void *malloc_aligned(size_t size, size_t alignment = 64);

template <class T1, class T2>
T1 ceil_aligned(T1 value, T2 alignment) {
    return value + alignment - (((value + alignment - 1) % alignment) + 1);
}

template <class T1, class T2>
T1 ceil_divide(T1 dividend, T2 alignment) {
    return (dividend + alignment - 1) / alignment;
}

template <class T1, class T2>
T1 floor_aligned(T1 value, T2 alignment) {
    return value - (value % alignment);
}

template <class T1, class T2>
T1 ceil_modulo(T1 value, T2 alignment) {
    T1 x = (value + alignment - 1) % alignment;
    return value + alignment - ((x < 0 ? x + alignment : x) + 1);
}

inline bool divides(int64_t x, int64_t y) {
    return y % x == 0;
}

int gcd(int x, int y);

typedef uint64_t ticks_t;
ticks_t secs_to_ticks(float secs);
ticks_t get_ticks();
time_t get_secs();
int64_t get_ticks_res();
double ticks_to_secs(ticks_t ticks);

// HEY: Maybe debugf and log_call and TRACEPOINT should be placed in
// debug.hpp (and debug.cc).
/* Debugging printing API (prints current thread in addition to message) */
#ifndef NDEBUG
void debugf(const char *msg, ...) __attribute__((format (printf, 1, 2)));
#else
#define debugf(...) ((void)0)
#endif

class rng_t {
public:
// Returns a random number in [0, n).  Is not perfectly uniform; the
// bias tends to get worse when RAND_MAX is far from a multiple of n.
    int randint(int n);
    explicit rng_t(int seed = -1);
private:
    struct drand48_data buffer_;
    DISABLE_COPYING(rng_t);
};

int randint(int n);
std::string rand_string(int len);

bool begins_with_minus(const char *string);
// strtoul() and strtoull() will for some reason not fail if the input begins
// with a minus sign. strtou64_strict() does.  Also we fix the constness of the
// end parameter.
int64_t strtoi64_strict(const char *string, const char **end, int base);
uint64_t strtou64_strict(const char *string, const char **end, int base);

// These functions return false and set the result to 0 if the conversion fails or
// does not consume the whole string.
MUST_USE bool strtoi64_strict(const std::string &str, int base, int64_t *out_result);
MUST_USE bool strtou64_strict(const std::string &str, int base, uint64_t *out_result);

std::string strprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
std::string vstrprintf(const char *format, va_list ap);

/* `demangle_cpp_name()` attempts to de-mangle the given symbol name. If it
succeeds, it returns the result as a `std::string`. If it fails, it throws
`demangle_failed_exc_t`. */
struct demangle_failed_exc_t : public std::exception {
    const char *what() const throw () {
        return "Could not demangle C++ name.";
    }
};
std::string demangle_cpp_name(const char *mangled_name);

// formatted time:
// yyyy-mm-dd hh:mm:ss   (19 characters)
const size_t formatted_time_length = 19;    // not including null

void format_time(time_t time, char* buf, size_t max_chars);
std::string format_time(time_t time);

time_t parse_time(const std::string &str) THROWS_ONLY(std::runtime_error);

/* Printing binary data to stdout in a nice format */

void print_hd(const void *buf, size_t offset, size_t length);

// Fast string compare

int sized_strcmp(const char *str1, int len1, const char *str2, int len2);


/* The home thread mixin is a mixin for objects that can only be used
on a single thread. Its thread ID is exposed as the `home_thread()`
method. Some subclasses of `home_thread_mixin_t` can move themselves to
another thread, modifying the field real_home_thread. */

#define INVALID_THREAD (-1)

class home_thread_mixin_t {
public:
    int home_thread() const { return real_home_thread; }

#ifndef NDEBUG
    void assert_thread() const;
#else
    void assert_thread() const { }
#endif  // NDEBUG

protected:
    explicit home_thread_mixin_t(int specified_home_thread);
    home_thread_mixin_t();
    virtual ~home_thread_mixin_t() { }

    int real_home_thread;

private:
    // Things with home threads should not be copyable, since we don't
    // want to nonchalantly copy their real_home_thread variable.
    DISABLE_COPYING(home_thread_mixin_t);
};

/* `on_thread_t` switches to the given thread in its constructor, then switches
back in its destructor. For example:

    printf("Suppose we are on thread 1.\n");
    {
        on_thread_t thread_switcher(2);
        printf("Now we are on thread 2.\n");
    }
    printf("And now we are on thread 1 again.\n");

*/

class on_thread_t : public home_thread_mixin_t {
public:
    explicit on_thread_t(int thread);
    ~on_thread_t();
};

void print_backtrace(FILE *out = stderr, bool use_addr2line = true);

template <class InputIterator, class UnaryPredicate>
bool all_match_predicate(InputIterator begin, InputIterator end, UnaryPredicate f) {
    bool res = true;
    for (; begin != end; begin++) {
        res &= f(*begin);
    }
    return res;
}

template <class T, class UnaryPredicate>
bool all_in_container_match_predicate (const T &container, UnaryPredicate f) {
    return all_match_predicate(container.begin(), container.end(), f);
}

bool notf(bool x);

std::string read_file(const char *path);

struct path_t {
    std::vector<std::string> nodes;
    bool is_absolute;
};

path_t parse_as_path(const std::string &);
std::string render_as_path(const path_t &);


#endif // UTILS_HPP_
