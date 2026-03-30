#pragma once

#include "hub.hpp"

#include "utils.hpp"

/**
 * @brief Closes both ends of the input and output pipes used for CGI communication.
 */
void		closePipes(int pipe_in[2], int pipe_out[2]);
