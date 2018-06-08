#include <type_traits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "iso8601.hpp"

char* itoa (int64_t i) {
    const int INT_DIGITS = 19; /* enough for 64 bit integer */
    static char buf[INT_DIGITS + 2]; /* Room for INT_DIGITS digits, - and '\0' */
    char *p = buf + INT_DIGITS + 1;   /* points to terminating '\0' */
    if (i >= 0) {
        do {
            *--p = '0' + (i % 10);
            i /= 10;
        } while (i != 0);
        return p;
    }
    else {            /* i < 0 */
        do {
            *--p = '0' - (i % 10);
            i /= 10;
        } while (i != 0);
        *--p = '-';
    }
    return p;
}

template <std::size_t N>
struct tag_t: std::integral_constant<std::size_t, N>{
    inline static char* apply2(char* in, int value) {
        size_t i;
        auto* buff = itoa(value);
        auto len = strlen(buff);
        for (i = 0; i < 2 - len; i++) *(in++) = '0';
        for (i = 0; i < len; i++) *(in++) = *(buff++);
        return in;
    }
};

struct tag_year     : tag_t<4> {
    inline static char* apply(char* in, datetime& dt) {
        size_t i;
        auto* buff = itoa(dt.year);
        auto* orig_ptr = in;
        auto len = strlen(buff );
        if (dt.year >= 0 && dt.year <= 999) {
            for (i = 0; i < 4 - len; i++) *(in++) = '0';
        }
        for (i = 0; i < len; i++) *(orig_ptr++) = *(buff++);
        return orig_ptr;
    }
};
struct tag_month    : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.mon); }};
struct tag_day      : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.mday); }};
struct tag_hour     : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.hour); }};
struct tag_min      : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.min); }};
struct tag_sec      : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.sec); }};
struct tag_ampm     : tag_t<2> { inline static char* apply(char* in, datetime& dt) {
    *(in++) = dt.hour < 12 ? 'A' : 'P';
    *(in++) = 'M';
    return in;
}};

template <char C>
struct tag_char : tag_t<1> {
    inline static char* apply(char* in, datetime& dt) {
        *in = C;
        return ++in;
    }
};


template <typename ...Ts> struct size_of_t;
template <typename T> struct size_of_t<T> { static const auto Value = T::value; };
template <typename T, typename ...Ts> struct size_of_t<T, Ts ...> {
    static const auto Value = (size_of_t<T>::Value + size_of_t<Ts...>::Value);
};

template <typename ... Tags> struct Composer;
template <typename T1> struct Composer<T1> {
    static char* fn(char* in, datetime& dt) {
        return T1::apply(in, dt);
    }
};
template <typename T, typename ...Tags> struct Composer<T, Tags...> {
    static char* fn(char* in, datetime& dt) {
        return Composer<Tags...>::fn(Composer<T>::fn(in, dt), dt);
    }
    static char* compose(char* in, datetime& dt) {
        return fn(in, dt);
    }
};

template <typename ...Args>
struct expression_t {
    static const auto N  = size_of_t<Args...>::Value;
    using FinalComposer = Composer<Args...>;

    inline static char* apply(char* in, datetime& dt) {
        return FinalComposer::compose(in,dt);
    }
};

using iso_t = expression_t<
    tag_year, tag_char<'-'>, tag_month, tag_char<'-'>, tag_day, tag_char<' '>,
    tag_hour, tag_char<':'>, tag_min, tag_char<':'>, tag_sec
>;


template <typename G>
void parse(const char* sample, bool expected) {
    datetime dt {0, 0, 0, 0, 0, 0};
    parser::microsec_t mksec {0};
    parser::timezone_t tz { parser::tz_info_t::LOCAL, 1, 0, 0 };
    parser::context_t ctx {dt, mksec, tz, 0, 0, 0, parser::time_unit_t::NONE };
    const char* end = sample + strlen(sample);
    const char* result = G::parse(sample, end, ctx);
    bool r = (result == end);
    if (r == expected) {
        std::cout << "ok ";
    } else {
        std::cout << "[!] ";
    }
    if (!r) {
        std::cout << "failed to parse '" << sample << "'\n";
    } else {
        bool utc = tz.tz_info != parser::tz_info_t::LOCAL;
        std::cout << "sample '" << sample << "' parsed. "
            << "y = " << dt.year << ", m = " << dt.mon << ", d = " << dt.mday
            << ", c.week = " << ctx.week << ", c.week_day= " << ctx.week_day
            << ", h = " << dt.hour << ", min = " << dt.min << ", sec = " << dt.sec
            << ", mksec = " << mksec;
        if (utc) {
            std::cout << ", UTC offset: " << (tz.sign > 0 ? '+' : '-')
                << tz.hour << ":" << tz.minute;
        } else {
            std::cout << ", [localtime]";
        }
        std::cout << "\n";
    }
}


int zmain(int argc, char** argv) {
    char buff[iso_t::N] = {0};
    datetime now { 2018, 4, 6, 22, 42, 5};
    char* buff_end = iso_t::apply(buff, now);
    auto sz = buff_end - buff;
    *buff_end = 0;
    std::cout << "result " << sz << "/" << iso_t::N << " bytes :: " << buff << "\n";
    parse<parser::grammar_date>("2018", true);
    parse<parser::grammar_date>("20181231", true);
    parse<parser::grammar_date>("2018-12", true);
    parse<parser::grammar_date>("2018-12-31", true);
    parse<parser::grammar_date>("201812", false);

    parse<parser::grammar_date>("20181", false);
    parse<parser::grammar_date>("201", false);
    parse<parser::grammar_date>("2018-256", true);
    parse<parser::grammar_date>("2018256", true);
    parse<parser::grammar_date>("2018-xxx", false);
    parse<parser::grammar_date>("2018W06", true);
    parse<parser::grammar_date>("2018W061", true);
    parse<parser::grammar_date>("2018-W06", true);
    parse<parser::grammar_date>("2018-W06-1", true);

    parse<parser::grammar_time>("12", true);
    parse<parser::grammar_time>("1213", true);
    parse<parser::grammar_time>("121314", true);
    parse<parser::grammar_time>("12:13", true);
    parse<parser::grammar_time>("12:13:14", true);
    parse<parser::grammar_time>("12.5", true);
    parse<parser::grammar_time>("12.30.5", false);
    parse<parser::grammar_time>("1230.5", true);
    parse<parser::grammar_time>("12:30.5", true);
    parse<parser::grammar_time>("12:30,5", true);
    parse<parser::grammar_time>("12:30:11.5", true);
    parse<parser::grammar_time>("12:30:11.500000", true);
    parse<parser::grammar_time>("12:30:11.555555", true);
    parse<parser::grammar_time>("12:30:11.5x5555", false);
    parse<parser::grammar_time>("12:30:11.5555555", false);

    parse<parser::grammar_vCard>("--0412", true);
    parse<parser::grammar_vCard>("---12", true);
    parse<parser::grammar_vCard>("--xxxx", false);
    parse<parser::grammar_vCard>("---xx", false);

    parse<parser::grammar_tz>("Z", true);
    parse<parser::grammar_tz>("+04", true);
    parse<parser::grammar_tz>("-0317", true);
    parse<parser::grammar_tz>("+03:17", true);

    parse<parser::grammar_iso8601>("2017-01-02T03:04:05", true);
    parse<parser::grammar_iso8601>("20170828T134935", true);
    parse<parser::grammar_iso8601>("20170828T134935Z", true);
    parse<parser::grammar_iso8601>("2017-01-02T03:04:05+05", true);
    parse<parser::grammar_iso8601>("2017-01-02T03:04:05-06:30", true);

    parse<parser::grammar_generic>("2013-03", true);
    parse<parser::grammar_generic>("2013-03-05", true);
    parse<parser::grammar_generic>("2013-03-05 17:38:26.068865+03", true);
    parse<parser::grammar_generic>("2013-03-05 17:38:26", true);
    parse<parser::grammar_generic>("2013-03-05 17:38:26+03", true);
    parse<parser::grammar_generic>("2013-03-05 3:4:5", true);
    parse<parser::grammar_generic>("2013-03-05 17:38:26+03:30", true);
    parse<parser::grammar_generic>("2013/03/05 17:38:26.068865+03", true);
    parse<parser::grammar_generic>("+123456789/03/05 17:38:26.068865+03", true);
    parse<parser::grammar_generic>("-123456789/03/05 17:38:26.068865+03", true);

    return 0;
}

template <typename G>
void perf_parse(const char* sample) {
    datetime dt {0, 0, 0, 0, 0, 0};
    parser::microsec_t mksec {0};
    parser::timezone_t tz { parser::tz_info_t::LOCAL, 1, 0, 0 };
    parser::context_t ctx {dt, mksec, tz, 1, 0, 0, parser::time_unit_t::NONE };
    const char* end = sample + strlen(sample);
    parser::grammar_generic::parse(sample, end, ctx);
};

int main(int argc, char** argv) {
    for (auto i = 0; i < 100000000; ++i) {
        perf_parse<parser::grammar_generic>("2013-03-05 17:38:26");
        //perf_parse<parser::grammar_iso8601>("2017-01-02T03:04:05+05");
    }
    return 0;
}
