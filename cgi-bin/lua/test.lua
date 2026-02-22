#!/usr/bin/env lua

-- CGI Headers
print("Content-Type: text/html")
print("")

-- HTML Content
print([[
<!DOCTYPE html>
<html>
<head>
    <title>Lua CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #000080; }
        .info { background: #f0f0f0; padding: 15px; border-radius: 5px; }
        .success { color: green; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🌙 Lua CGI Test</h1>
    <p class="success">✓ Lua CGI is working!</p>

    <div class="info">
        <h2>Environment Variables:</h2>
        <ul>
]])

-- Display some CGI environment variables
local env_vars = {
    "REQUEST_METHOD",
    "QUERY_STRING",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "SERVER_SOFTWARE",
    "GATEWAY_INTERFACE",
    "SCRIPT_NAME",
    "PATH_INFO"
}

for _, var in ipairs(env_vars) do
    local value = os.getenv(var) or "(not set)"
    print(string.format("<li><strong>%s:</strong> %s</li>", var, value))
end

print([[
        </ul>
        <h2>Lua Version:</h2>
        <p>]] .. _VERSION .. [[</p>
    </div>
</body>
</html>
]])
