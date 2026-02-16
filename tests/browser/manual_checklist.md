# Browser Testing Checklist

This document provides a manual testing checklist for webserv using standard web browsers.

## Setup

1. Start the server: `./webserv config/default.conf`
2. Open browsers: Chrome, Firefox, Safari, etc.
3. Navigate to: `http://localhost:8080`

## Basic Functionality Tests

### Static Content Serving
- [ ] **Homepage loads** - Visit `http://localhost:8080/`
  - Verify page loads completely
  - Check for proper rendering
  - Inspect browser console for errors

- [ ] **HTML files** - Request various HTML files
  - Check Content-Type header (text/html)
  - Verify character encoding
  - Test relative links

- [ ] **CSS files** - Load stylesheets
  - Verify Content-Type header (text/css)
  - Check if styles are applied
  - Test @import directives

- [ ] **JavaScript files** - Load JS resources
  - Verify Content-Type header (application/javascript)
  - Check if scripts execute
  - Test inline vs external scripts

- [ ] **Images** - Load various image formats
  - PNG: Check rendering and transparency
  - JPEG: Verify quality
  - GIF: Test animations
  - SVG: Check vector rendering
  - Proper Content-Type for each format

- [ ] **Favicon** - Check `http://localhost:8080/favicon.ico`
  - Verify icon appears in browser tab

### Directory Listing
- [ ] **Root directory** - `http://localhost:8080/`
- [ ] **Subdirectory** - `http://localhost:8080/uploads/`
- [ ] **Empty directory** - Create and test
- [ ] **Permission denied** - If applicable

### Error Pages
- [ ] **404 Not Found** - Request non-existent page
  - `http://localhost:8080/nonexistent.html`
  - Verify custom 404 page loads
  - Check status code in Developer Tools

- [ ] **403 Forbidden** - If applicable
  - Check proper error page
  - Verify status code

- [ ] **500 Internal Server Error** - Trigger with CGI error
  - `http://localhost:8080/cgi-bin/error_test.py`
  - Verify custom 500 page
  - Check status code

- [ ] **405 Method Not Allowed** - Use unsupported method
  - Use browser extensions to send TRACE, etc.

## File Upload Tests

1. **Open upload test page**: `http://localhost:8080/upload_test.html`

### Small File Upload
- [ ] **Text file** (< 1KB)
  - Select file
  - Click upload
  - Verify success message
  - Check uploaded file exists in uploads/ directory

- [ ] **Image file** (< 1MB)
  - Upload a small PNG/JPEG
  - Verify upload success
  - Check file integrity

### Medium File Upload
- [ ] **Document file** (1-10MB)
  - Upload PDF or large image
  - Monitor upload progress
  - Verify completion

### Large File Upload
- [ ] **Large file** (50-100MB)
  - Create test file: `dd if=/dev/urandom of=large.bin bs=1M count=50`
  - Upload via browser
  - Verify server handles it (may reject based on config)
  - Check server doesn't crash

### Edge Cases
- [ ] **Multiple simultaneous uploads**
  - Open upload page in multiple tabs
  - Upload different files simultaneously
  - Verify all complete successfully

- [ ] **Empty file upload**
  - Create empty file
  - Attempt upload
  - Check server response

- [ ] **Filename with special characters**
  - Upload file named: `test file (1) & [2].txt`
  - Verify proper handling

- [ ] **Very long filename**
  - Create file with 200+ character name
  - Attempt upload

## CGI Tests

### Python CGI
- [ ] **Basic CGI script** - `http://localhost:8080/cgi-bin/test.py`
  - Verify output renders correctly
  - Check response headers

- [ ] **CGI with query params** - `http://localhost:8080/cgi-bin/test.py?name=John&age=25`
  - Verify parameters are processed
  - Check output

- [ ] **POST to CGI** - Use upload form posting to CGI
  - Verify POST data handled correctly

### Shell CGI
- [ ] **Shell script** - `http://localhost:8080/cgi-bin/test.sh`
  - Verify execution
  - Check environment variables

### PHP CGI (if supported)
- [ ] **PHP script** - `http://localhost:8080/cgi-bin/test.php`
  - Verify PHP execution
  - Check phpinfo() output

## Browser-Specific Features

### Developer Tools Checks
- [ ] **Network tab**
  - Monitor request/response headers
  - Check status codes
  - Verify timing information
  - Check caching behavior

- [ ] **Console tab**
  - Verify no JavaScript errors
  - Check for CSP violations
  - Monitor CORS issues (if applicable)

### Caching
- [ ] **First visit** - Clear cache, visit homepage
  - Note load time
  - Check network requests

- [ ] **Subsequent visit** - Reload page
  - Verify cached resources (304 Not Modified)
  - Check If-Modified-Since header

- [ ] **Force refresh** - Ctrl+Shift+R or Cmd+Shift+R
  - Verify all resources re-downloaded
  - Check Cache-Control headers

### Connection Persistence
- [ ] **Keep-Alive connections**
  - Load page with multiple resources
  - Check Network tab: verify same connection ID
  - Monitor connection reuse

- [ ] **Connection close**
  - Check "Connection: close" handling
  - Verify new connection established when needed

### Forms
- [ ] **GET form** - Submit form with GET method
  - Verify query parameters in URL
  - Check server handles request

- [ ] **POST form** - Submit form with POST
  - Verify POST data transmission
  - Check server response

- [ ] **Form with multiple fields**
  - Text inputs, checkboxes, radio buttons
  - Verify all data received by server

### Special Characters in URLs
- [ ] **Spaces** - `http://localhost:8080/path%20with%20spaces`
- [ ] **Unicode** - `http://localhost:8080/文件`
- [ ] **Special chars** - `http://localhost:8080/file?param=value%26more`

## Performance Tests

### Multiple Tabs
- [ ] **Open 10 tabs** - All loading homepage
  - Verify all load successfully
  - Check server doesn't slow down excessively

- [ ] **Open 50 tabs** - Stress test
  - Monitor server performance
  - Check for crashes or hangs

### Large Page Loads
- [ ] **Page with many resources**
  - Create HTML with 100+ images
  - Load in browser
  - Verify all resources load
  - Check for timeout issues

### Slow Network Simulation
- [ ] **Chrome DevTools** - Enable throttling
  - Set to "Slow 3G"
  - Load pages
  - Verify graceful handling
  - Check timeout behavior

## Cross-Browser Testing

Test the same scenarios across multiple browsers:

### Chrome/Chromium
- [ ] All basic functionality tests
- [ ] Upload tests
- [ ] Developer tools verification

### Firefox
- [ ] All basic functionality tests
- [ ] Upload tests
- [ ] Developer tools verification
- [ ] Note any differences from Chrome

### Safari (macOS/iOS)
- [ ] Basic functionality
- [ ] Upload tests
- [ ] Mobile Safari specific issues

### Edge
- [ ] Basic functionality
- [ ] Upload tests

### curl (Command Line)
- [ ] Basic GET requests
- [ ] POST requests with data
- [ ] Headers inspection
- [ ] Compare with browser behavior

## Comparison with Reference Server

**Optional**: Compare behavior side-by-side with NGINX

1. Start NGINX on port 8081 with similar config
2. Open two browser windows
3. Test same scenarios on both servers
4. Compare:
   - Response headers
   - Status codes
   - Error pages
   - Performance
   - Caching behavior

## Issues Found

Document any issues discovered during testing:

| Issue | Browser | URL | Expected | Actual | Severity |
|-------|---------|-----|----------|--------|----------|
|       |         |     |          |        |          |

## Notes

- Test both HTTP/1.0 and HTTP/1.1 when possible
- Check server logs for errors during testing
- Monitor server resource usage (CPU, memory)
- Use browser extensions for advanced testing (modify headers, etc.)
- Test with AdBlock/privacy extensions enabled
- Test with browser caching disabled
- Test with JavaScript disabled

## Completion

Date: _______________
Tester: _____________
Browsers tested: ____________________
Pass rate: ______%
Critical issues: ____
