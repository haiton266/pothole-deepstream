## This code designs for Backend Local Server. Written by FastAPI Framework

Configuration Tips
- Number of Workers: Set the number of workers to 2 * CPU cores + 1 for optimal performance.
- Environment Variables: Use environment variables for configuration (e.g., WORKERS, PORT).
- Reverse Proxy: Use a reverse proxy like Nginx or Traefik in front of Gunicorn for SSL termination, load balancing, and static file serving.

Runing command in `run.sh`