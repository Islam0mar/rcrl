#include <algorithm>
#include <cstdint>
#include <experimental/type_traits>
#include <iostream>
#include <string>
#include <vector>

template <typename T>
constexpr auto type_name() noexcept {
  std::string_view name = "Error: unsupported compiler", prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl type_name<";
  suffix = ">(void) noexcept";
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using VS = std::vector<const char*>;
void __print(int x) { cerr << x; }
void __print(long x) { cerr << x; }
void __print(long long x) { cerr << x; }
void __print(unsigned x) { cerr << x; }
void __print(unsigned long x) { cerr << x; }
void __print(unsigned long long x) { cerr << x; }
void __print(float x) { cerr << x; }
void __print(double x) { cerr << x; }
void __print(long double x) { cerr << x; }
void __print(char x) { cerr << '\'' << x << '\''; }
void __print(char* x) { cerr << '\"' << x << '\"'; }
void __print(string x) { cerr << '\"' << x << '\"'; }
void __print(bool x) { cerr << (x ? "true" : "false"); }

template <typename T, typename V>
void __print(const std::pair<T, V>& x) {
  cerr << '{';
  __print(x.first);
  cerr << ',';
  __print(x.second);
  cerr << '}';
}
template <typename T>
inline void __print(const T* x);
template <typename T>
inline void __print(T x);

template <typename T, bool = false>
struct Print {
  void operator()(T x) { __print(x); }
};
template <typename T>
struct Print<T, true> {
  void operator()(T x) { cerr << x; }
};

template <typename T>
inline void __print(const T* x) {
  auto w = const_cast<T*>(x);
  __print(w);
}

template <typename T>
using print_operator_t =
    decltype(std::declval<std::ostream&>() << std::declval<const T&>());

template <typename T>
inline void __print(T x) {
  int f = 0;
  cerr << '{';
  for (auto& i : x)
    cerr << (f++ ? "," : ""),
        Print<decltype(i), std::experimental::is_detected<
                               print_operator_t, decltype(i)>::value>()(i);
  ;
  cerr << "}";
}

void _print() { cerr << "]\n"; }
template <typename T, size_t N, typename... V>
void _print(T const (&x)[N], V... v) {
  Print<T, std::experimental::is_detected<print_operator_t, T>::value>()(x);
  if (sizeof...(v)) cerr << ", ";
  _print(v...);
}
template <typename T, typename... V>
void _print(T t, V... v) {
  Print<T, std::experimental::is_detected<print_operator_t, T>::value>()(t);
  if (sizeof...(v)) cerr << ", ";
  _print(v...);
}
#ifndef DEBUG_MODE
#define DEBUG(x...)             \
  cerr << "[" << #x << "] = ["; \
  _print(x)
#else
#define DEBUG(x...)
#endif
