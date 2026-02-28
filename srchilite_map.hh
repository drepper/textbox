#ifndef SRCHILITE_MAP_HH_
# define SRCHILITE_MAP_HH_ 1

# include <optional>
# include <string>

namespace widget {

  struct source_highlight_data {
    std::string fname;
    std::string stdname;
  };

  extern std::optional<source_highlight_data> find_source_highlight_data(const std::string& s);

} // namespace widget

#endif // srchilite_map.hh
