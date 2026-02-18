#ifndef CGIUTILS_HPP
# define CGIUTILS_HPP

# include <iostream>
# include <unistd.h>
# include "utils.hpp"

/**
 * Writes the entire buffer to the specified file descriptor,
 * handling partial writes and EINTR errors.
 */
int			loopingWrite(int fd, const char* buffer, size_t length);
/**
 * Closes both ends of the input and output pipes used for CGI communication.
 */
void		closePipes(int pipe_in[2], int pipe_out[2]);

#endif
