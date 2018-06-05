#include <type_traits>
#include <cstdint>
#include <cstring>
#include <iostream>

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

struct datetime {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
};

template <std::size_t N>
struct tag_t: std::integral_constant<std::size_t, N>{
    inline static char* apply2(char* in, int value) {
        int i;
        auto* buff = itoa(value);
        auto len = strlen(buff);
        for (i = 0; i < 2 - len; i++) *(in++) = '0';
        for (i = 0; i < len; i++) *(in++) = *(buff++);
        return in;
    }
};

struct tag_year     : tag_t<4> {
    inline static char* apply(char* in, datetime& dt) {
        int i;
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
struct tag_month    : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.month); }};
struct tag_day      : tag_t<2> { inline static char* apply(char* in, datetime& dt) { return apply2(in, dt.day); }};
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


int main(int argc, char** argv) {
    char buff[iso_t::N] = {0};
    datetime now { 2018, 4, 6, 22, 42, 5};
    char* buff_end = iso_t::apply(buff, now);
    auto sz = buff_end - buff;
    *buff_end = 0;
    std::cout << "result " << sz << "/" << iso_t::N << " bytes :: " << buff << "\n";
    return 0;
}
