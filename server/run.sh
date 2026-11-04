#!/bin/bash
# The server on port ${PORT:-8098}; it serves the client from ../client/dist too.
cd "$(dirname "$0")"
exec ./build/server
