#ifndef UTILS_HPP
# define UTILS_HPP

# include <filesystem>
# include <fstream>
# include <iostream>
# include <unistd.h>

std::string	loadFile(const std::string &path);
int			loopingWrite(int fd, const char* buffer, size_t length);

#endif
