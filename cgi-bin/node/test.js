#!/usr/bin/env node

// CGI Headers
console.log("Content-Type: text/html");
console.log("");

// HTML Content
console.log(`
<!DOCTYPE html>
<html>
<head>
    <title>Node.js CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #68a063; }
        .info { background: #f0f0f0; padding: 15px; border-radius: 5px; }
        .success { color: green; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🚀 Node.js CGI Test</h1>
    <p class="success">✓ Node.js CGI is working!</p>

    <div class="info">
        <h2>Environment Variables:</h2>
        <ul>
`);

// Display CGI environment variables
const envVars = [
    "REQUEST_METHOD",
    "QUERY_STRING",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "SERVER_SOFTWARE",
    "GATEWAY_INTERFACE",
    "SCRIPT_NAME",
    "PATH_INFO"
];

envVars.forEach(varName => {
    const value = process.env[varName] || "(not set)";
    console.log(`            <li><strong>${varName}:</strong> ${value}</li>`);
});

console.log(`
        </ul>
        <h2>Node.js Version:</h2>
        <p>${process.version}</p>

        <h2>Process Info:</h2>
        <ul>
            <li><strong>Platform:</strong> ${process.platform}</li>
            <li><strong>Architecture:</strong> ${process.arch}</li>
            <li><strong>PID:</strong> ${process.pid}</li>
        </ul>
    </div>
</body>
</html>
`);
