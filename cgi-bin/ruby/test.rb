#!/usr/bin/env ruby

# CGI Headers
puts "Content-Type: text/html"
puts ""

# HTML Content
puts <<~HTML
  <!DOCTYPE html>
  <html>
  <head>
      <title>Ruby CGI Test</title>
      <style>
          body { font-family: Arial, sans-serif; margin: 40px; }
          h1 { color: #cc342d; }
          .info { background: #f0f0f0; padding: 15px; border-radius: 5px; }
          .success { color: green; font-weight: bold; }
      </style>
  </head>
  <body>
      <h1>💎 Ruby CGI Test</h1>
      <p class="success">✓ Ruby CGI is working!</p>

      <div class="info">
          <h2>Environment Variables:</h2>
          <ul>
HTML

# Display CGI environment variables
env_vars = %w[
  REQUEST_METHOD
  QUERY_STRING
  CONTENT_TYPE
  CONTENT_LENGTH
  SERVER_SOFTWARE
  GATEWAY_INTERFACE
  SCRIPT_NAME
  PATH_INFO
]

env_vars.each do |var|
  value = ENV[var] || "(not set)"
  puts "              <li><strong>#{var}:</strong> #{value}</li>"
end

puts <<~HTML
          </ul>
          <h2>Ruby Version:</h2>
          <p>#{RUBY_VERSION}</p>

          <h2>Ruby Info:</h2>
          <ul>
              <li><strong>Platform:</strong> #{RUBY_PLATFORM}</li>
              <li><strong>Patchlevel:</strong> #{RUBY_PATCHLEVEL}</li>
              <li><strong>Release Date:</strong> #{RUBY_RELEASE_DATE}</li>
          </ul>
      </div>
  </body>
  </html>
HTML
