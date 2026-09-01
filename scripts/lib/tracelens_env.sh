#!/usr/bin/env bash
# Shared TraceLens runtime configuration.
# Source this file from repository scripts; it derives all service endpoints
# from the root .env while allowing explicit URL overrides for proxies.

if [[ -z "${TRACELENS_ROOT:-}" ]]; then
  TRACELENS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fi

tracelens_load_env() {
  local env_file="${TRACELENS_ENV_FILE:-$TRACELENS_ROOT/.env}"
  if [[ -f "$env_file" ]]; then
    # Parse dotenv assignments without eval/source: JSON values and URLs are
    # data, and must never become shell syntax.
    local line key value
    while IFS= read -r line || [[ -n "$line" ]]; do
      line="${line%$'\r'}"
      [[ -z "${line//[[:space:]]/}" || "${line#\#}" != "$line" ]] && continue
      [[ "$line" != *=* ]] && continue
      key="${line%%=*}"
      value="${line#*=}"
      [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
      value="${value#${value%%[![:space:]]*}}"
      value="${value%${value##*[![:space:]]}}"
      if [[ ${!key+x} != x ]]; then
        if [[ ${#value} -ge 2 && ${value:0:1} == \" && ${value: -1} == \" ]]; then
          value="${value:1:${#value}-2}"
        elif [[ ${#value} -ge 2 && ${value:0:1} == \"'\" && ${value: -1} == \"'\" ]]; then
          value="${value:1:${#value}-2}"
        fi
        printf -v "$key" '%s' "$value"
        export "$key"
      fi
    done < "$env_file"
  fi

  : "${HTTP_SERVER_HOST:=0.0.0.0}"
  : "${HTTP_SERVER_PORT:=8080}"
  : "${PYTHON_HTTP_HOST:=0.0.0.0}"
  : "${PYTHON_HTTP_PORT:=8090}"
  : "${CS_HOST:=${HOST:-0.0.0.0}}"
  : "${CS_PORT:=${PORT:-8091}}"
  : "${WEB_DEV_HOST:=127.0.0.1}"
  : "${WEB_DEV_PORT:=3000}"

  : "${CPP_BACKEND_URL:=http://${HTTP_SERVER_HOST}:${HTTP_SERVER_PORT}}"
  : "${PYTHON_SERVICE_URL:=http://${PYTHON_HTTP_HOST}:${PYTHON_HTTP_PORT}}"
  : "${CS_SERVICE_URL:=http://${CS_HOST}:${CS_PORT}}"

  export TRACELENS_ROOT HTTP_SERVER_HOST HTTP_SERVER_PORT
  export PYTHON_HTTP_HOST PYTHON_HTTP_PORT CS_HOST CS_PORT
  export WEB_DEV_HOST WEB_DEV_PORT CPP_BACKEND_URL PYTHON_SERVICE_URL CS_SERVICE_URL
}

tracelens_load_env
