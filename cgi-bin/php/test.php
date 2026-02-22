#!/usr/bin/php-cgi
<?php
/**
 * Simple PHP CGI test script
 */

// CGI headers are automatically handled by php-cgi
?>
Content-Type: text/html

<!DOCTYPE html>
<html>
<head>
    <title>PHP CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #8e44ad; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #8e44ad; color: white; }
    </style>
</head>
<body>
    <h1>🐘 PHP CGI Test - Success!</h1>

    <h2>Request Information</h2>
    <table>
        <tr><th>Property</th><th>Value</th></tr>
        <tr><td>Request Method</td><td><?php echo $_SERVER['REQUEST_METHOD'] ?? 'N/A'; ?></td></tr>
        <tr><td>Query String</td><td><?php echo $_SERVER['QUERY_STRING'] ?? 'N/A'; ?></td></tr>
        <tr><td>Content Length</td><td><?php echo $_SERVER['CONTENT_LENGTH'] ?? '0'; ?></td></tr>
        <tr><td>Server Name</td><td><?php echo $_SERVER['SERVER_NAME'] ?? 'N/A'; ?></td></tr>
        <tr><td>Server Port</td><td><?php echo $_SERVER['SERVER_PORT'] ?? 'N/A'; ?></td></tr>
    </table>

    <?php if (!empty($_GET)): ?>
    <h2>GET Parameters</h2>
    <table>
        <tr><th>Key</th><th>Value</th></tr>
        <?php foreach ($_GET as $key => $value): ?>
        <tr>
            <td><?php echo htmlspecialchars($key); ?></td>
            <td><?php echo htmlspecialchars($value); ?></td>
        </tr>
        <?php endforeach; ?>
    </table>
    <?php endif; ?>

    <?php if ($_SERVER['REQUEST_METHOD'] === 'POST' && !empty(file_get_contents('php://input'))): ?>
    <h2>POST Data</h2>
    <pre><?php echo htmlspecialchars(file_get_contents('php://input')); ?></pre>
    
    <?php
    // Try to parse as form data
    parse_str(file_get_contents('php://input'), $post_params);
    if (!empty($post_params)):
    ?>
    <table>
        <tr><th>Key</th><th>Value</th></tr>
        <?php foreach ($post_params as $key => $value): ?>
        <tr>
            <td><?php echo htmlspecialchars($key); ?></td>
            <td><?php echo htmlspecialchars($value); ?></td>
        </tr>
        <?php endforeach; ?>
    </table>
    <?php endif; ?>
    <?php endif; ?>

    <h2>PHP & Server Information</h2>
    <table>
        <tr><td><strong>PHP Version</strong></td><td><?php echo phpversion(); ?></td></tr>
        <tr><td><strong>Server Software</strong></td><td><?php echo $_SERVER['SERVER_SOFTWARE'] ?? 'N/A'; ?></td></tr>
        <tr><td><strong>Gateway Interface</strong></td><td><?php echo $_SERVER['GATEWAY_INTERFACE'] ?? 'N/A'; ?></td></tr>
        <tr><td><strong>Server Protocol</strong></td><td><?php echo $_SERVER['SERVER_PROTOCOL'] ?? 'N/A'; ?></td></tr>
    </table>

</body>
</html>
