#pragma once

#include "hub.hpp"

#include "Logger.hpp"

std::string	loadFile(const std::string &path);
/**
 * @brief Decodes an URL-encoded string.
 *
 * Converts percent-encoded bytes (for example, %20) and '+' characters
 * to their decoded form. Encoded forward slashes (%2F / %2f) are kept as-is
 * to preserve path boundaries.
 *
 * @param str Input URL-encoded string.
 * @return Decoded string.
 */
std::string	urlDecode(const std::string &str);
/**
 * Safely closes a file descriptor and sets it to -1 to prevent accidental reuse.
 */
void		safeClose(int &fd);
