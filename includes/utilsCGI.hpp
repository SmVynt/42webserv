#ifndef UTILSCGI_HPP
# define UTILSCGI_HPP

# include <iostream>
# include <unistd.h>

/**
 * Writes the entire buffer to the specified file descriptor,
 * handling partial writes and EINTR errors.
 */
int			loopingWrite(int fd, const char* buffer, size_t length);
/**
 * Closes both ends of the input and output pipes used for CGI communication.
 */
void		closePipes(int pipe_in[2], int pipe_out[2]);
/**
 * Safely closes a file descriptor and sets it to -1 to prevent accidental reuse.
 */
void		safeClose(int &fd);

#endif
