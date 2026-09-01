// Browser-facing service configuration.
// Vite only exposes VITE_* variables to client code. In production, the C++
// service is normally the page origin; Python and C/S remain explicit origins.

const env = import.meta.env || {};

function pageOrigin() {
  if (typeof window === 'undefined' || !window.location) return 'http://localhost';
  return window.location.origin;
}

function pageEndpoint(port) {
  if (typeof window === 'undefined' || !window.location) {
    return `http://localhost:${port}`;
  }
  const protocol = window.location.protocol === 'https:' ? 'https:' : 'http:';
  return `${protocol}//${window.location.hostname}:${port}`;
}

export const API_BASE_URL = env.VITE_API_BASE_URL || '';
export const CPP_BASE_URL = env.VITE_CPP_API_URL
  || (env.VITE_CPP_PORT ? pageEndpoint(env.VITE_CPP_PORT) : pageOrigin());
export const PYTHON_API_BASE_URL = env.VITE_PYTHON_API_URL
  || pageEndpoint(env.VITE_PYTHON_PORT || '8090');
export const CS_API_BASE_URL = env.VITE_CS_API_URL
  || pageEndpoint(env.VITE_CS_PORT || '8091');

export const CPP_WS_BASE_URL = env.VITE_CPP_WS_URL
  || CPP_BASE_URL.replace(/^http:/, 'ws:').replace(/^https:/, 'wss:');

export const SERVICE_TIMEOUTS = {
  cpp: Number(env.VITE_CPP_TIMEOUT_MS || 30000),
  python: Number(env.VITE_PYTHON_TIMEOUT_MS || 120000),
  cs: Number(env.VITE_CS_TIMEOUT_MS || 30000),
};

export const SERVICE_PORTS = {
  cpp: env.VITE_CPP_PORT || '',
  python: env.VITE_PYTHON_PORT || '8090',
  cs: env.VITE_CS_PORT || '8091',
};
