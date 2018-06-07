#include <algorithm>
#include <cmath>

struct datetime {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
};


namespace parser {

enum class time_unit_t { NONE = 0, H, M, S};
enum class tz_info_t { LOCAL, UTC };

using microsec_t = uint32_t;

struct timezone_t {
    tz_info_t tz_info;
    int sign;
    uint32_t hour;
    uint32_t minute;
};

struct context_t {
    datetime& dt;
    microsec_t& mksec;
    timezone_t& tz;
    uint32_t week;
    uint32_t week_day;
    time_unit_t time_unit;
};

// handlers

struct handler_year {
    static inline void handle(int value, context_t& ctx) {
        ctx.dt.year = value;
    }
};

struct handler_month {
    static inline void handle(int value, context_t& ctx) {
        ctx.dt.month = value;
    }
};

struct handler_day {
    static inline void handle(int value, context_t& ctx) {
        ctx.dt.day = value;
    }
};

struct handler_week {
    static inline void handle(int value, context_t& ctx) {
        ctx.week = value;
    }
};

struct handler_week_day {
    static inline void handle(int value, context_t& ctx) {
        ctx.week_day = value;
    }
};

struct handler_hour {
    static inline void handle(int value, context_t& ctx) {
        ctx.time_unit = time_unit_t::H;
        ctx.dt.hour = value;
    }
};

struct handler_minute {
    static inline void handle(int value, context_t& ctx) {
        ctx.time_unit = time_unit_t::M;
        ctx.dt.min = value;
    }
};

struct handler_second {
    static inline void handle(int value, context_t& ctx) {
        ctx.time_unit = time_unit_t::S;
        ctx.dt.sec = value;
    }
};

struct handler_tz_utc {
    static inline void handle(int value, context_t& ctx) {
        ctx.tz.tz_info = tz_info_t::UTC;
    }
};

struct handler_tz_offset_sign {
    static inline void handle(int value, context_t& ctx) {
        ctx.tz.tz_info = tz_info_t::UTC;
        ctx.tz.sign = (value == '+') ? 1 : -1;
    }
};

struct handler_tz_offset_hour {
    static inline void handle(int value, context_t& ctx) {
        ctx.tz.tz_info = tz_info_t::UTC;
        ctx.tz.hour = value;
    }
};

struct handler_tz_offset_minute {
    static inline void handle(int value, context_t& ctx) {
        ctx.tz.tz_info = tz_info_t::UTC;
        ctx.tz.minute = value;
    }
};

struct handler_fraction {
    static inline void handle(int value, int count, context_t& ctx) {
        if (value) {
            double share = value / std::pow(10, count);
            if (ctx.time_unit == time_unit_t::H) {
                ctx.dt.min = (uint32_t)(share * 60);
                //const double sec_left = (share * 3600) - ctx.dt.min * 60;
                const double sec_left = (share - ((double)ctx.dt.min)/60) * 3600;
                ctx.dt.sec = (uint32_t)sec_left;
                const double mksec_left = (share * 3600) * 1000000 - (((double)(ctx.dt.min) * 60 * 1000000) + (double)(ctx.dt.sec) * 1000000);
                ctx.mksec = (uint32_t)mksec_left;
            } else if (ctx.time_unit == time_unit_t::M) {
                ctx.dt.sec = (uint32_t)(share * 60);
                //const double mksec_left = (share * 1000000) - ctx.dt.min * 1000000;
                const double mksec_left = (share - ((double)ctx.dt.sec)/60) * 1000000;
                ctx.mksec = (uint32_t)mksec_left;
            } else if (ctx.time_unit == time_unit_t::S) {
                ctx.mksec = (uint32_t)(share * 1000000);
            }
        }
    }
};


using handler_ordinal_date = handler_day;

// OR
template <typename ...Ts> struct op_or;
template <typename T> struct op_or<T> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        return T::parse(ptr, ptr_end, ctx);
    }
};
template <typename T, typename ...Ts> struct op_or<T, Ts...> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        const char* ptr_next = op_or<T>::parse(ptr, ptr_end, ctx);
        if (ptr_next) return ptr_next;
        return op_or<Ts...>::parse(ptr, ptr_end, ctx);
    }
};

// SEQ

template <typename ...Ts> struct op_seq;
template <typename T> struct op_seq<T> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        return T::parse(ptr, ptr_end, ctx);
    }
};
template <typename T, typename ...Ts> struct op_seq<T, Ts...> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        const char* ptr_next = op_seq<T>::parse(ptr, ptr_end, ctx);
        if (!ptr_next) return NULL;
        return op_seq<Ts...>::parse(ptr_next, ptr_end, ctx);
    }
};


// maybe

template <typename ...Ts> struct op_maybe;
template <typename T> struct op_maybe<T> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        if (ptr < ptr_end) {
            const char* ptr_next = T::parse(ptr, ptr_end, ctx);
            return ptr_next ? ptr_next : ptr;
        }
        return ptr;
    }
};
template <typename T, typename ...Ts> struct op_maybe<T, Ts...> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        if (ptr < ptr_end) {
            const char* ptr_next = T::parse(ptr, ptr_end, ctx);          // direct non-recursive call
            if (ptr_next >= ptr) return ptr_next;                        // stop chaining
            return op_maybe<Ts...>::parse(ptr, ptr_end, ctx);            // continue chaining if 1st op was NOT successfull
        }
        return ptr;
    }
};

// terms

template <char T, typename Handler = void>
struct term_char {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        if (ptr - ptr_end == 0) return NULL;
        if (*ptr == T) {
            Handler::handle(T, ctx);
            return ptr + 1;
        }
        return NULL;
    }
};

template <char T>
struct term_char<T, void> {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        if (ptr - ptr_end == 0) return NULL;
        if (*ptr == T) return ptr + 1;
        return NULL;
    }
};




template <int N, typename Handler>
struct term_number {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        const char* number_end = ptr + N;
        if (number_end > ptr_end) return NULL;
        int value = 0;
        while(ptr != number_end) {
            char digit = (*ptr++) - '0';
            if (digit >= 0 && digit <= 9) {
                value = (value * 10) + digit;
            } else {
                return NULL;
            }
        }
        Handler::handle(value, ctx);
        return number_end;
    }
};

// parses [1...N] digits
template <int N, typename Handler>
struct term_var_number {
    static inline const char* parse(const char* ptr, const char* ptr_end, context_t& ctx) {
        const char* number_end = std::min(ptr + N, ptr_end);
        if (ptr >= ptr_end) return NULL;
        int value = 0;
        const char* start = ptr;
        while(ptr != number_end) {
            char digit = (*ptr) - '0';
            if (digit >= 0 && digit <= 9) {
                value = (value * 10) + digit;
            } else {
                break;
            }
            ++ptr;
        }
        if (ptr == start) return NULL;
        auto count = ptr - start;
        Handler::handle(value, count, ctx);
        return ptr;
    }
};



using term_year         = term_number<4, handler_year>;
using term_month        = term_number<2, handler_month>;
using term_day          = term_number<2, handler_day>;
using term_week         = term_number<2, handler_week>;
using term_week_day     = term_number<1, handler_week_day>;
using term_ordinal_date = term_number<3, handler_ordinal_date>;
using term_hour         = term_number<2, handler_hour>;
using term_min          = term_number<2, handler_minute>;
using term_sec          = term_number<2, handler_second>;
using term_tz_UTC       = term_char<'Z', handler_tz_utc>;
using term_hour_tz      = term_number<2, handler_tz_offset_hour>;
using term_min_tz       = term_number<2, handler_tz_offset_minute>;
using term_fraction_p   = term_var_number<6, handler_fraction>;
using term_fraction     = op_seq<op_or<term_char<'.'>, term_char<','>>, term_fraction_p>;
template<char T> using term_tz_sing = term_char<T, handler_tz_offset_sign>;
template<typename B> using term_fraq = op_seq<B, term_fraction>;



using grammar_date = op_seq<
    term_year,                                                                                                      // YYYY
    op_maybe<
        op_or<
            op_seq<term_month, term_day>,                                                                           // YYYYMMDD,
            op_seq<term_char<'-'>, op_or<
                term_ordinal_date,                                                                                  // YYYY-DDD
                op_seq<term_month, op_maybe<op_seq<term_char<'-'>, term_day>>>,                                     // YYYY-MM, YYYY-MM-DD
                op_seq<term_char<'W'>, term_week, op_maybe<op_seq<term_char<'-'>, term_week_day>>>
            >>,
            op_seq<term_char<'W'>, term_week, op_maybe< term_week_day>>,                                            // YYYYWww, YYYYWwwD
            term_ordinal_date                                                                                       // YYYYDDD
        >
    >
>;

using grammar_vCard = op_seq<
    term_char<'-'>, term_char<'-'>,
    op_or<
        op_seq<term_month, term_day>,                                           // --MMDD
        op_seq<term_char<'-'>, term_day>                                        // ---DD
    >
>;

using grammar_time = op_seq<
    term_hour,                                                                  // HH
    op_maybe<
        op_or<
            op_seq<term_min, op_maybe<                                          // HHMM
                term_sec,                                                       // HHMMSS
                op_maybe<term_fraction>                                         // HHMM.M{1,6}
            >>,
            op_seq<term_char<':'>, term_min,                                    // HH:MM
                op_maybe<
                    op_seq<term_char<':'>, term_sec, op_maybe<term_fraction>>,  // HH:MM:SS, HH:MM:SS.S{1,6}
                    term_fraction                                               // HH:MM.M{1,6}
            >>,
            term_fraction                                                       // HH.H{1,6}
        >
    >
>;

using grammar_tz_offset = op_seq<
    term_hour_tz,
    op_maybe<
        term_min_tz,
        op_seq<term_char<':'>,  term_min_tz>
    >
>;

using grammar_tz = op_or<
    term_tz_UTC,                                                                // Z
    op_seq<
        op_or<term_tz_sing<'+'>, term_tz_sing<'-'>>,                            // ±HH:MM, ±HHMM, ±HH
        grammar_tz_offset
    >
>;


using grammar = op_or<
    op_seq<
        grammar_date,
        op_maybe<
            op_seq<
                term_char<'T'>, grammar_time, op_maybe<grammar_tz>
            >
        >
    >,
    grammar_vCard
>;

using grammar_generic = op_seq<
    term_year,
    op_or<term_char<'-'>, term_char<'/'>>,
    term_month,
    op_or<term_char<'-'>, term_char<'/'>>,
    term_day,                                                                       /// YYYY-MM-DD, YYYY/MM/DD
    op_maybe<op_seq<
        term_char<' '>,
        term_hour,
        term_char<':'>,
        term_min,
        term_char<':'>,
        term_sec,                                                                   // HH:MM:SS
        op_maybe<op_seq<
            term_fraction,                                                          // .s{1,6}
            op_maybe<op_seq<
                op_or<term_tz_sing<'+'>, term_tz_sing<'-'>>,                        // ±HH:MM, ±HHMM, ±HH
                grammar_tz_offset
            >>
        >>,
        op_maybe<op_seq<
            op_or<term_tz_sing<'+'>, term_tz_sing<'-'>>,                            // ±HH:MM, ±HHMM, ±HH
            grammar_tz_offset
        >>
    >>
>;

}
