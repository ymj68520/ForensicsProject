"""
Application configuration.

Centralizes all runtime settings behind a single :class:`Settings` instance
loaded from environment variables / a local ``.env`` file via
`pydantic-settings <https://docs.pydantic.dev/latest/concepts/pydantic_settings/>`_.

Every module that needs configuration imports the module-level :data:`settings`
singleton rather than reading ``os.getenv`` directly, so the source of truth
stays in one place. Defaults are tuned for local development; production
deployments override values through environment variables.
"""
import json
import os
from pathlib import Path
from typing import List, Optional

from pydantic_settings import BaseSettings, SettingsConfigDict


def find_env_file() -> Optional[Path]:
    """从当前目录向上查找项目 .env（与 httpserver.config.find_env_file 一致）。

    server 常从 python_service/ 目录启动，写死的 ".env" 相对路径会找不到
    项目根的 .env，导致 DATABASE_URL 等静默落到开发默认值。
    """
    current = Path.cwd()
    while current != current.parent:
        env_path = current / ".env"
        if env_path.exists():
            return env_path
        current = current.parent
    return None


def _read_env_value(env_path: Optional[Path], key: str) -> str:
    if env_path is None:
        return ""
    try:
        for line in env_path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line.startswith(f"{key}="):
                return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return ""


def _cors_origins_from_env() -> List[str]:
    """解析 PYTHON_CORS_ORIGINS（JSON 数组），与 httpserver 行为一致。

    生产 Web UI 由 C++ 服务(:8666)托管，浏览器跨源直连本服务；若只白名单
    Vite 开发端口，预检会被拒（"Disallowed CORS origin"）。
    """
    raw = os.getenv("PYTHON_CORS_ORIGINS", "").strip() or _read_env_value(
        find_env_file(), "PYTHON_CORS_ORIGINS"
    ).strip()
    if raw:
        try:
            parsed = json.loads(raw)
            if isinstance(parsed, list) and all(isinstance(o, str) for o in parsed):
                return parsed
        except (json.JSONDecodeError, ValueError):
            pass
    return [
        "http://localhost:5173",
        "http://localhost:3000",
        "http://127.0.0.1:5173",
    ]


class Settings(BaseSettings):
    """Application settings.

    Values are populated in this order (later overrides earlier):
    constructor kwargs > environment variables > ``.env`` file > field defaults.
    Field names are case-sensitive and must match the env var exactly.
    """

    # Application
    APP_NAME: str = "TraceLens Server"
    APP_VERSION: str = "1.0.0"
    DEBUG: bool = False
    ENVIRONMENT: str = os.getenv("ENVIRONMENT", "development")

    # Server (distributed C/S backend).
    # Port 8091, NOT 8090: the legacy python_service/httpserver (LLM/graphiti
    # proxy for the local-mode C++ 8080 server) already owns 8090. Dual-stack
    # deployments run both simultaneously; see the port map in
    # docs/superpowers/plans/2026-07-27-cs-integration-hardening.md.
    HOST: str = os.getenv("HOST", "0.0.0.0")
    PORT: int = int(os.getenv("PORT", "8091"))

    # Database
    DATABASE_URL: str = os.getenv(
        "DATABASE_URL",
        "postgresql://postgres:postgres@localhost:5432/tracelens",
    )
    # Keep driver, pool checkout, and lifespan budgets independent.  The
    # driver timeout must be shorter than DB_STARTUP_TIMEOUT because cancelling
    # a worker thread cannot interrupt an in-flight socket operation.
    DB_CONNECT_TIMEOUT: int = int(os.getenv("DB_CONNECT_TIMEOUT", "5"))
    DB_POOL_TIMEOUT: int = int(os.getenv("DB_POOL_TIMEOUT", "5"))
    DB_STARTUP_TIMEOUT: float = float(os.getenv("DB_STARTUP_TIMEOUT", "30"))

    # JWT
    JWT_SECRET_KEY: str = os.getenv("JWT_SECRET_KEY", "change-this-in-production")
    JWT_ALGORITHM: str = os.getenv("JWT_ALGORITHM", "HS256")
    USER_TOKEN_EXPIRE_HOURS: int = 1
    CLIENT_TOKEN_EXPIRE_DAYS: int = 30

    # CORS
    CORS_ORIGINS: List[str] = _cors_origins_from_env()

    # File Upload
    MAX_UPLOAD_SIZE: int = 5 * 1024 * 1024 * 1024  # 5GB
    MAX_LLM_TEXT_SIZE: int = 10 * 1024 * 1024  # 10MB

    # Command Queue
    DEFAULT_POLL_INTERVAL: int = 10  # seconds
    MIN_POLL_INTERVAL: int = 5
    MAX_POLL_INTERVAL: int = 30
    DEFAULT_COMMAND_TTL_HOURS: int = 24
    CRITICAL_COMMAND_TTL_HOURS: int = 1

    # Organization
    DEFAULT_ORGANIZATION_NAME: str = "Default Organization"
    DEFAULT_SUBSCRIPTION_TIER: str = "enterprise"

    # extra="ignore": the distributed server shares the repo-root ``.env``
    # with the legacy python_service/httpserver (dual-stack deployment). That
    # file carries httpserver-only vars (GRAPHITI_*, DB_NAME, LOG_LEVEL, ...);
    # pydantic-settings rejects unknown keys sourced from an ``env_file``
    # (unlike keys from os.environ, which it silently ignores), so without
    # this the server fails to boot whenever the shared .env is present.
    # Mirror httpserver/config.py's own extra="ignore".
    model_config = SettingsConfigDict(env_file=find_env_file(), case_sensitive=True, extra="ignore")


settings = Settings()
