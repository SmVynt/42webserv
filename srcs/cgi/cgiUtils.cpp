#include "cgiUtils.hpp"

int	loopingWrite(int fd, const char* buffer, size_t length) {
	size_t	total_written = 0;
	size_t	write_size = 4096;
	while (total_written < length) {
		size_t to_write = std::min(write_size, length - total_written);
		ssize_t bytes_written = write(fd, buffer + total_written, to_write);
		if (bytes_written == -1) {
			if (errno == EINTR)
				continue; // Retry on interrupt
			std::cerr << "Error: write() failed in loopingWrite" << std::endl;
			return -1;
		}
		total_written += bytes_written;
	}
	return total_written;
}

void	closePipes(int pipe_in[2], int pipe_out[2]) {
	safeClose(pipe_in[0]);
	safeClose(pipe_in[1]);
	safeClose(pipe_out[0]);
	safeClose(pipe_out[1]);
}
