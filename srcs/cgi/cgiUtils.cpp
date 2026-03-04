#include "cgiUtils.hpp"

void	closePipes(int pipe_in[2], int pipe_out[2]) {
	safeClose(pipe_in[0]);
	safeClose(pipe_in[1]);
	safeClose(pipe_out[0]);
	safeClose(pipe_out[1]);
}
