#!/bin/bash
# Start FastAPI app with Gunicorn and Uvicorn workers

export WORKERS=4
export PORT=8000

gunicorn main:app \
    --workers $WORKERS \
    --worker-class uvicorn.workers.UvicornWorker \
    --bind 0.0.0.0:$PORT