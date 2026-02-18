#ifndef UTILS_HPP
# define UTILS_HPP

# include <filesystem>
# include <fstream>
# include <iostream>
# include <unistd.h>
# include "Logger.hpp"

std::string	loadFile(const std::string &path);
/**
 * Safely closes a file descriptor and sets it to -1 to prevent accidental reuse.
 */
void		safeClose(int &fd);

#endif
