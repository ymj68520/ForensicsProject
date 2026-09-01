#!/bin/bash
#
# 启动Python服务（使用虚拟环境）
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"
TRACELENS_ROOT="$PROJECT_ROOT"
# shellcheck disable=SC1091
source "$TRACELENS_ROOT/scripts/lib/tracelens_env.sh"

# Optional download proxy for flaky networks. Set PIP_PROXY in the environment
# or .env; it is exported by the shared configuration loader.
if [ -n "${PIP_PROXY:-}" ]; then
    export HTTP_PROXY="$PIP_PROXY" HTTPS_PROXY="$PIP_PROXY"
    echo -e "${YELLOW}使用下载代理${NC}: $PIP_PROXY"
fi

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}启动 Python HTTP 服务${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# 检查虚拟环境
if [ ! -d "python_service/.venv" ]; then
    echo "虚拟环境不存在，正在创建..."
    python3 -m venv python_service/.venv
fi

# 激活虚拟环境并安装依赖
echo "检查依赖..."
# Only run pip install if core packages are missing, to avoid hitting the
# network (and failing on transient IncompleteRead errors) on every start.
# Probe both stacks — fastapi (httpserver) and sqlalchemy (distributed C/S
# server) — so a partial venv triggers a top-up of BOTH requirements files.
if ! python_service/.venv/bin/python -c "import fastapi, uvicorn, pydantic, sqlalchemy" 2>/dev/null; then
    python_service/.venv/bin/pip install -q --retries 3 \
        -r python_service/httpserver/requirements.txt \
        -r python_service/requirements.txt
else
    echo "核心依赖已安装，跳过 pip install"
fi

# 检查端口占用
if lsof -i :"$PYTHON_HTTP_PORT" > /dev/null 2>&1; then
    echo -e "${YELLOW}端口 ${PYTHON_HTTP_PORT} 已被占用，正在关闭旧进程...${NC}"
    lsof -ti :"$PYTHON_HTTP_PORT" | xargs -r kill -9
    sleep 1
fi

# 启动服务
echo "启动 Python HTTP 服务（端口 ${PYTHON_HTTP_PORT}）..."
# IMPORTANT: run from python_service/ with PYTHONPATH pointing at it, matching
# start_all_services.sh. graphiti_integration/ is a sibling of httpserver/, so
# it is only importable when python_service/ is on sys.path. Launching from the
# project root without PYTHONPATH (the old behaviour) raised
# "No module named 'graphiti_integration'" at startup.
PYTHON_SERVICE_DIR="$PROJECT_ROOT/python_service"
PYTHON_EXEC="$PYTHON_SERVICE_DIR/.venv/bin/python"
cd "$PYTHON_SERVICE_DIR"
PYTHONPATH="$PYTHON_SERVICE_DIR:$PYTHONPATH" \
    "$PYTHON_EXEC" -m httpserver.main

