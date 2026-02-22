#!/usr/bin/env perl

use strict;
use warnings;

# CGI Headers
print "Content-Type: text/html\n\n";

# HTML Content
print <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <title>Perl CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #0073a1; }
        .info { background: #f0f0f0; padding: 15px; border-radius: 5px; }
        .success { color: green; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🐪 Perl CGI Test</h1>
    <p class="success">✓ Perl CGI is working!</p>

    <div class="info">
        <h2>Environment Variables:</h2>
        <ul>
HTML

# Display CGI environment variables
my @env_vars = qw(
    REQUEST_METHOD
    QUERY_STRING
    CONTENT_TYPE
    CONTENT_LENGTH
    SERVER_SOFTWARE
    GATEWAY_INTERFACE
    SCRIPT_NAME
    PATH_INFO
);

foreach my $var (@env_vars) {
    my $value = $ENV{$var} // "(not set)";
    print "            <li><strong>$var:</strong> $value</li>\n";
}

print "        </ul>\n";
print "        <h2>Perl Version:</h2>\n";
print "        <p>$]</p>\n";
print "        <h2>Executable:</h2>\n";
print "        <p>$^X</p>\n";
print <<'HTML';
    </div>
</body>
</html>
HTML
