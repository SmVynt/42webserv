#pragma once
#include <poll.h>
#include <vector>
#include <string>
#include <map>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include <filesystem>
#include "Config.hpp"
#include <algorithm>
#include "Request.hpp"
#include "Response.hpp"
#include "utils.hpp"
#include "Logger.hpp"
#include "Methods.hpp"
#include <csignal>
#include "CGI.hpp"
#include "Session.hpp"

/**
 * @enum FDType
 * @brief Classifies a file descriptor's role inside the poll() event loop.
 */
enum FDType{
	FD_LISTENER,	///< Listening socket: accepts new incoming connections
	FD_CLIENT,		///< Client socket: handles HTTP requests and responses
	FD_CGI_IN,		///< CGI input pipe: writes POST body to the script's stdin
	FD_CGI_OUT		///< CGI output pipe: reads the script's execution result
};

/**
 * @enum ClientState
 * @brief Tracks the current phase of a client connection's lifecycle.
 */
enum ClientState{
	STATE_READING,		///< Receiving and parsing the HTTP request
	STATE_PROCESSING,	///< Request complete, waiting for CGI or handler result
	STATE_WRITING		///< Sending the HTTP response back to the client
};

/**
 * @struct FDMetadata
 * @brief Per-descriptor state attached to every FD registered with poll().
 *
 * Holds all context needed to service a single file descriptor across
 * multiple non-blocking I/O cycles: identity, timeouts, request/response
 * objects, CGI executor, and session binding.
 */
struct FDMetadata{
	uint64_t		generation;				///< Monotonic ID; detects stale events after FD reuse
	int				fd;						///< The raw file descriptor number
	FDType			type;					///< Role of this descriptor (listener / client / CGI pipe)
	ClientState		client_state;			///< Current phase for client FDs

	time_t			last_activity;			///< Timestamp of the last I/O for timeout logic
	int				timeout_value;			///< General inactivity timeout (seconds)
	int				timeout_reading_value;	///< Stricter timeout while waiting for request data

	int				port;					///< Listening port that accepted this client
	int				config_index;			///< Index into Cluster::_config_data for the matching server block
	int				client_fd;				///< Owning client FD (used by CGI pipes to find their parent)

	std::string		cgi_raw_output;			///< Accumulated raw bytes read from FD_CGI_OUT
	size_t			cgi_write_offset;		///< Bytes of POST body already written to FD_CGI_IN

	Request			request;				///< Parsed HTTP request for this connection
	Response		response;				///< HTTP response being built or sent
	CGIexecutor*	cgi_executor;			///< Owns the forked CGI child process (nullptr when inactive)

	std::string		session_id;				///< Session cookie value bound to this request
	Session*		session_ptr;			///< Non-owning pointer into Cluster::_active_sessions
	bool			is_new_session;			///< True if a fresh session was created for this request

	bool			needs_continue;			///< Pending "100 Continue" to send on next POLLOUT
};

/**
 * @class Cluster
 * @brief Core server engine: owns the poll() event loop, all sockets,
 *        and orchestrates request → response flow.
 *
 * A single Cluster instance binds one or more listener sockets (one per
 * unique host:port pair in the config), then enters a non-blocking poll()
 * loop that dispatches readable/writable events to the appropriate handler.
 *
 * Copying is deleted; exactly one instance exists, accessible via
 * cluster_reference() for signal handling.
 */
class Cluster {
	public:
		/** @brief Default constructor. Initialises generation counter and shutdown flag. */
		Cluster();

		/**
		 * @brief Constructs the cluster from parsed server configurations.
		 * @param config Vector of server blocks read from the configuration file.
		 */
		Cluster(const std::vector<ServerConfig>& config);

		Cluster(const Cluster& other) = delete;
		Cluster& operator=(const Cluster& other) = delete;

		/** @brief Destroys all CGI executors and closes every registered FD. */
		~Cluster();

		/**
		 * @brief Creates and binds listener sockets for each unique host:port.
		 *
		 * Sets SO_REUSEADDR, resolves the host via getaddrinfo, binds, listens,
		 * and registers each socket with poll() as FD_LISTENER.
		 *
		 * @note Throws std::runtime_error on any syscall failure.
		 */
		void	setupCluster();

		/**
		 * @brief Enters the main poll() event loop; returns only after shutdown.
		 *
		 * Each iteration: compacts the pollfd vector, calls poll(timeout=1s),
		 * snapshots events with generation stamps, runs timeout checks, then
		 * dispatches POLLIN / POLLOUT / POLLHUP to the type-specific handler.
		 *
		 * @note Safe against FD reuse within a single iteration thanks to
		 *       the generation counter on every FDMetadata.
		 */
		void	run();

		/**
		 * @brief Signals the event loop to stop after the current iteration.
		 * @note Safe to call from a signal handler (writes volatile sig_atomic_t).
		 */
		void	requestShutdown();

		/**
		 * @brief Looks up an active session by its cookie ID.
		 * @param sessionId The "session_id" value from the Cookie header.
		 * @return Pointer to the Session, or nullptr if not found / expired.
		 */
		Session*	findSessionById(const std::string& sessionId);

	private:
		// ── FD management ───────────────────────────────────────────────

		/**
		 * @brief Registers a new FD with poll() and creates its FDMetadata.
		 *
		 * Sets O_NONBLOCK via fcntl before registration.
		 *
		 * @param fd         Raw file descriptor to register.
		 * @param type       Role of the descriptor (listener / client / CGI pipe).
		 * @param client_ref For CGI pipes: the owning client FD; -1 otherwise.
		 * @param timeout    Inactivity timeout in seconds (0 = no timeout).
		 */
		void addFD(int fd, FDType type, int client_ref, int timeout);

		/**
		 * @brief Marks the FD as dead in _pollfds, closes it, and erases metadata.
		 * @param fd File descriptor to remove.
		 */
		void removeFD(int fd);

		/**
		 * @brief Like removeFD but skips close(); used when the CGI executor
		 *        owns the underlying pipe and will close it itself.
		 * @param fd File descriptor to unregister without closing.
		 */
		void removeFDNoClose(int fd);

		/**
		 * @brief Changes the events mask (POLLIN / POLLOUT / 0) for an FD.
		 * @param fd     Target file descriptor.
		 * @param events New poll event mask.
		 */
		void updatePollEvents(int fd, short events);

		/**
		 * @brief Refreshes the last_activity timestamp for timeout tracking.
		 * @param fd File descriptor whose activity to update.
		 */
		void updateActivity(int fd);

		/**
		 * @brief Scans _fd_table for stale connections and expired sessions.
		 *
		 * Connections exceeding their timeout are closed; clients stuck in
		 * STATE_READING beyond timeout_reading_value receive a 408 response.
		 * Delegates session cleanup to cleanupExpiredSessions().
		 */
		void handleTimeout();

		/**
		 * @brief Removes sessions from _active_sessions whose TTL has elapsed.
		 */
		void cleanupExpiredSessions();

		// ── Connection management ───────────────────────────────────────

		/**
		 * @brief Accepts a pending connection on a listener socket.
		 *
		 * Calls accept(), finds the matching ServerConfig by port, sets up
		 * FDMetadata with initial state STATE_READING, and records the
		 * client's IP via getnameinfo().
		 *
		 * @param listen_fd The listener socket that triggered POLLIN.
		 * @note Returns silently on EWOULDBLOCK / EAGAIN (edge-case with poll).
		 */
		void acceptNewConnection(int listen_fd);

		/**
		 * @brief Collects all CGI pipe FDs (in + out) linked to a given client.
		 * @param client_fd The client connection whose pipes to find.
		 * @return Vector of FD numbers (may be empty).
		 */
		std::vector<int> collectCgiPipes(int client_fd);

		/**
		 * @brief Tears down a connection and any associated CGI resources.
		 *
		 * If the FD is a CGI pipe, kills the executor and sends 504 to the
		 * owning client. If it is a client FD with a running CGI, cleans up
		 * orphan pipes first. Otherwise performs a simple removeFD.
		 *
		 * @param fd File descriptor to close.
		 */
		void closeConnection(int fd);

		/**
		 * @brief Resets a keep-alive client for the next request cycle.
		 *
		 * Preserves socket-level fields (port, config_index, timeout) and
		 * replaces Request/Response with fresh instances. Switches back to
		 * POLLIN / STATE_READING.
		 *
		 * @param fd Client file descriptor to recycle.
		 */
		void resetConnection(int fd);

		// ── Request processing ──────────────────────────────────────────

		/**
		 * @brief Reads available bytes from a client socket and drives the
		 *        request parser state machine.
		 *
		 * On a complete request, delegates to processCompletedRequest().
		 * On "Expect: 100-continue", flips to POLLOUT so the 100 response
		 * can be sent before the body arrives.
		 *
		 * @param fd Client file descriptor.
		 * @return true if the caller should skip further event handling for
		 *         this FD (connection was closed).
		 */
		bool handleClientRequest(int fd);

		/**
		 * @brief Handles a fully-parsed request: resolves config, session,
		 *        location, validates body size, then dispatches to CGI or
		 *        static handler.
		 *
		 * @param fd   Client file descriptor.
		 * @param data Reference to the client's FDMetadata.
		 */
		void processCompletedRequest(int fd, FDMetadata& data);

		/**
		 * @brief Checks whether the request body exceeds the configured limit.
		 *
		 * Compares both the actual body size and the Content-Length header
		 * against the location or server-level client_max_body_size.
		 * On violation, generates a 413 response.
		 *
		 * @param fd     Client file descriptor (for poll event update).
		 * @param data   Client's FDMetadata.
		 * @param loc    Matched location block (may be nullptr).
		 * @param config Server configuration for fallback limit.
		 * @return true if the body was rejected (413 response queued).
		 */
		bool validateBodySize(int fd, FDMetadata& data, const Location* loc, const ServerConfig& config);

		/**
		 * @brief Forks and executes a CGI script, registering its pipes with poll().
		 *
		 * On executor failure, sends a 500 response immediately. On success,
		 * registers FD_CGI_OUT for POLLIN and, if the request has a body,
		 * FD_CGI_IN for POLLOUT. The client FD is suspended (events = 0)
		 * until CGI completes.
		 *
		 * @param fd     Client file descriptor.
		 * @param data   Client's FDMetadata.
		 * @param loc    Matched location block containing CGI configuration.
		 * @param config Server configuration for timeout values.
		 */
		void launchCgiRequest(int fd, FDMetadata& data, const Location& loc, const ServerConfig& config);

		/**
		 * @brief Handles a non-CGI request via RequestHandler::handleRequest.
		 *
		 * Generates an error page if the handler returns a non-2xx status.
		 * Strips the body for HEAD requests. Attaches session cookie if new.
		 *
		 * @param fd     Client file descriptor.
		 * @param data   Client's FDMetadata.
		 * @param config Server configuration for error pages.
		 */
		void handleStaticRequest(int fd, FDMetadata& data, const ServerConfig& config);

		/**
		 * @brief Finds the best-matching server config for a Host header.
		 *
		 * Iterates _config_data for entries matching the port, then compares
		 * server_names. Returns the first name match, or the first port match
		 * as the default server.
		 *
		 * @param port Listening port the request arrived on.
		 * @param host Host header value (without port suffix).
		 * @return Index into _config_data, or -1 if no port matches at all.
		 */
		int resolveServerConfig(int port, const std::string& host);

		/**
		 * @brief Binds a session to the current request.
		 *
		 * Parses the "session_id" value from the Cookie header. If the
		 * session does not exist or the cookie is absent, creates a new
		 * session in _active_sessions and sets is_new_session = true.
		 *
		 * @param data Client's FDMetadata (updated with session_id,
		 *             session_ptr, is_new_session).
		 */
		void resolveSession(FDMetadata& data);

		// ── Response handling ───────────────────────────────────────────

		/**
		 * @brief Sends queued response bytes to the client socket.
		 *
		 * Handles the special case of a pending "100 Continue" first.
		 * After the full response is sent, either resets the connection
		 * for keep-alive or closes it.
		 *
		 * @param fd Client file descriptor.
		 * @return true if the connection was closed or reset (caller should
		 *         skip further event handling for this FD).
		 */
		bool handleClientResponse(int fd);

		/**
		 * @brief Builds an error Response with a custom or default HTML body.
		 *
		 * Checks the server config for a user-provided error page file;
		 * falls back to Response::makeDefaultError() if none is configured
		 * or the file cannot be loaded.
		 *
		 * @param code         HTTP status code.
		 * @param config_index Index into _config_data for error page lookup.
		 * @return Ready-to-prepare Response object.
		 */
		Response generateErrorResponse(int code, int config_index);

		// ── CGI pipeline ────────────────────────────────────────────────

		/**
		 * @brief Reads available data from a CGI stdout pipe.
		 *
		 * Appends bytes to cgi_raw_output. On EOF (bytes == 0) or read
		 * error, delegates to handleCgiEnd() to finalise the response.
		 *
		 * @param cgi_fd The FD_CGI_OUT file descriptor that triggered POLLIN.
		 */
		void handleCgiRead(int cgi_fd);

		/**
		 * @brief Writes POST body chunks to a CGI stdin pipe.
		 *
		 * Tracks progress via cgi_write_offset. Once all bytes are written,
		 * closes the pipe (signalling EOF to the child) and detaches it
		 * from the executor.
		 *
		 * @param cgi_in_fd The FD_CGI_IN file descriptor that triggered POLLOUT.
		 */
		void handleCgiWrite(int cgi_in_fd);

		/**
		 * @brief Finalises a CGI transaction after the output pipe is drained.
		 *
		 * Parses the raw CGI output into an HTTP response, checks the child's
		 * exit status for errors, attaches session cookie, cleans up all
		 * CGI pipes, kills the child process, and switches the client FD
		 * to STATE_WRITING / POLLOUT.
		 *
		 * @param cgi_fd The FD_CGI_OUT that reached EOF or errored.
		 */
		void handleCgiEnd(int cgi_fd);

		// ── Data members ────────────────────────────────────────────────

		std::vector<ServerConfig>	_config_data;		///< Parsed server blocks from the configuration file
		std::vector<struct pollfd>	_pollfds;			///< Active poll descriptors (compacted each iteration)
		std::map<int, FDMetadata>	_fd_table;			///< Per-FD metadata keyed by descriptor number
		uint64_t					_fd_generation;		///< Monotonic counter for FDMetadata::generation
		std::map<int, int>			_listen_sockets;	///< Listener FD → port mapping
		volatile sig_atomic_t		_shutdown;			///< Flag set by signal_handler to exit the event loop
		std::map<std::string, Session>	_active_sessions;	///< Live sessions keyed by session_id cookie value
};

/**
 * @brief Returns a reference to the global Cluster pointer (singleton accessor).
 *
 * Used by signal_handler() to reach the running Cluster instance
 * without global variables.
 *
 * @return Reference to the static Cluster* (initially nullptr).
 */
Cluster*&	cluster_reference();

/**
 * @brief SIGINT / SIGTERM handler; requests graceful shutdown.
 * @param sig Signal number received.
 * @note Only calls Cluster::requestShutdown() via cluster_reference().
 */
void		signal_handler(int sig);
