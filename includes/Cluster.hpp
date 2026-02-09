#pragma once
#include <poll.h>
class Cluster {
	public:
		Cluster();
		~Cluster();
	private:
		int _pollFd;
};
