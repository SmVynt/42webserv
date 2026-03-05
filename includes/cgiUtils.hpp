#ifndef CGIUTILS_HPP
# define CGIUTILS_HPP

# include <iostream>
# include <unistd.h>
# include "utils.hpp"

/**
 * Closes both ends of the input and output pipes used for CGI communication.
 */
void		closePipes(int pipe_in[2], int pipe_out[2]);

#endif
