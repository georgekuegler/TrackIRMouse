#ifndef TRACKIRMOUSE_TYPES_HPP
#define TRACKIRMOUSE_TYPES_HPP

#include <vector>

using Deg = double;
using Pixels = signed int;
using game_titles_t = std::vector<std::pair<std::string, std::string>>;

enum class retcode { success, fail };
enum class msgcode { normal, red_text, close_app };

#endif /* TRACKIRMOUSE_TYPES_HPP */
