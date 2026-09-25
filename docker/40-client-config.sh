#!/bin/sh
# Renders /usr/share/nginx/html/client-config.json from the environment at
# container start, so one image serves any deployment. Runs from the nginx
# image's /docker-entrypoint.d before nginx starts.
set -eu

url="${DARKEDEN_WEBSOCKET_URL:-/game}"
host="${DARKEDEN_LOGIN_HOST:-127.0.0.1}"
port="${DARKEDEN_LOGIN_PORT:-9999}"

# The launcher rejects a URL with a query, fragment or credentials, and the
# transport allows only host-name characters; fail here rather than in the
# browser.
case "$url" in
    *'?'*|*'#'*|*'@'*|*'"'*|*'\'*|'') echo "client-config: invalid DARKEDEN_WEBSOCKET_URL '$url'" >&2; exit 1 ;;
esac
if ! printf '%s' "$host" | grep -Eq '^[A-Za-z0-9.-]{1,253}$'; then
    echo "client-config: invalid DARKEDEN_LOGIN_HOST '$host'" >&2
    exit 1
fi
if ! printf '%s' "$port" | grep -Eq '^[1-9][0-9]{0,4}$' || [ "$port" -gt 65535 ]; then
    echo "client-config: invalid DARKEDEN_LOGIN_PORT '$port'" >&2
    exit 1
fi

cat > /usr/share/nginx/html/client-config.json <<EOF
{
  "websocketUrl": "$url",
  "loginHost": "$host",
  "loginPort": $port
}
EOF
echo "client-config: websocketUrl=$url loginHost=$host loginPort=$port"
