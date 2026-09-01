import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig(({ mode }) => {
  const env = {
    ...loadEnv(mode, process.cwd(), ''),
    ...loadEnv(mode, '..', ''),
  };
  const cppTarget = env.VITE_CPP_PROXY_TARGET
    || env.CPP_BACKEND_URL
    || `http://${env.HTTP_SERVER_HOST || '127.0.0.1'}:${env.HTTP_SERVER_PORT || '8080'}`;
  const pythonTarget = env.VITE_PYTHON_PROXY_TARGET
    || env.PYTHON_SERVICE_URL
    || `http://${env.PYTHON_HTTP_HOST || '127.0.0.1'}:${env.PYTHON_HTTP_PORT || '8090'}`;
  const csTarget = env.VITE_CS_PROXY_TARGET
    || env.CS_SERVICE_URL
    || `http://${env.CS_HOST || env.HOST || '127.0.0.1'}:${env.CS_PORT || env.PORT || '8091'}`;

  return {
    plugins: [react()],
    define: {
      // Make root .env service ports available to browser code without
      // exposing the complete server environment to the client bundle.
      'import.meta.env.VITE_CPP_PORT': JSON.stringify(env.VITE_CPP_PORT || env.HTTP_SERVER_PORT || ''),
      'import.meta.env.VITE_PYTHON_PORT': JSON.stringify(env.VITE_PYTHON_PORT || env.PYTHON_HTTP_PORT || '8090'),
      'import.meta.env.VITE_CS_PORT': JSON.stringify(env.VITE_CS_PORT || env.CS_PORT || env.PORT || '8091'),
      'import.meta.env.VITE_CPP_API_URL': JSON.stringify(env.VITE_CPP_API_URL || ''),
      'import.meta.env.VITE_PYTHON_API_URL': JSON.stringify(env.VITE_PYTHON_API_URL || ''),
      'import.meta.env.VITE_CS_API_URL': JSON.stringify(env.VITE_CS_API_URL || ''),
    },
    test: {
      environment: 'jsdom',
      globals: true,
      setupFiles: './src/test/setup.js',
      css: true,
    },
    server: {
      host: env.WEB_DEV_HOST || '127.0.0.1',
      port: Number(env.VITE_DEV_SERVER_PORT || env.WEB_DEV_PORT || 3000),
      proxy: {
        '/csapi': {
          target: csTarget,
          changeOrigin: true,
          rewrite: (path) => path.replace(/^\/csapi/, ''),
        },
        '/tasks': { target: cppTarget, changeOrigin: true },
        '/api/reports': { target: pythonTarget, changeOrigin: true },
        '/api/graphiti': { target: pythonTarget, changeOrigin: true },
        '/api/llm': { target: pythonTarget, changeOrigin: true },
        '/api/office': { target: pythonTarget, changeOrigin: true },
        '/api/db': { target: pythonTarget, changeOrigin: true },
        '/api/wechat': { target: pythonTarget, changeOrigin: true },
        '/api/investigation': { target: pythonTarget, changeOrigin: true },
        '/api': { target: cppTarget, changeOrigin: true },
      },
    },
    build: {
      outDir: 'dist',
      assetsDir: 'assets',
      sourcemap: true,
      rollupOptions: {
        output: {
          manualChunks: {
            'react-vendor': ['react', 'react-dom', 'react-router-dom'],
            'd3-vendor': ['d3'],
            'redux-vendor': ['@reduxjs/toolkit', 'react-redux'],
          },
        },
      },
    },
  };
});
