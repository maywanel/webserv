#!/usr/bin/env python3
import datetime

# CGI scripts MUST print this header first
print("Content-Type: text/html\r\n\r\n")

server_now = datetime.datetime.now()
server_iso = server_now.isoformat()

print(f"""<!doctype html>
<html>
<head>
    <meta charset="UTF-8" />
    <title>CGI Time</title>
    <style>
        body {{
            font-family: Arial, sans-serif;
            background: #0f172a;
            color: #e2e8f0;
            margin: 0;
            padding: 2rem;
        }}
        .card {{
            max-width: 800px;
            margin: 0 auto;
            background: #1e293b;
            border: 1px solid #334155;
            border-radius: 14px;
            padding: 1.5rem;
            box-shadow: 0 10px 24px rgba(0,0,0,.35);
            text-align: center;
        }}
        h1 {{ margin-top: 0; color: #93c5fd; }}
        .clock {{
            font-size: 5rem;
            font-weight: 800;
            color: #22d3ee;
            margin: 2rem 0;
            line-height: 1.1;
        }}
    </style>
</head>
<body>
    <div class="card">
        <h1>Hello from Python!</h1>
        <p>Current server time (live):</p>
        <div id="clock" class="clock">--:--:--</div>
    </div>

    <script>
        // Start from server time at render, then tick every second
        let current = new Date("{server_iso}");

        function pad(n) {{
            return String(n).padStart(2, "0");
        }}

        function formatTime(d) {{
            return pad(d.getHours()) + ":" +
                   pad(d.getMinutes()) + ":" +
                   pad(d.getSeconds());
        }}

        function updateClock() {{
            document.getElementById("clock").textContent = formatTime(current);
            current = new Date(current.getTime() + 1000);
        }}

        updateClock();
        setInterval(updateClock, 1000);
    </script>
</body>
</html>
""")
