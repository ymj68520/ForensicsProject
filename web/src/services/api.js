import axios from 'axios';
import {
  API_BASE_URL,
  CPP_BASE_URL,
  PYTHON_API_BASE_URL,
  CS_API_BASE_URL,
  SERVICE_TIMEOUTS,
} from '../config/runtime';

// C++ 后端 API 客户端
const api = axios.create({
  baseURL: API_BASE_URL,
  headers: { 'Content-Type': 'application/json' },
  timeout: SERVICE_TIMEOUTS.cpp,
});

// Python 服务 API 客户端
const pythonApi = axios.create({
  baseURL: PYTHON_API_BASE_URL,
  headers: { 'Content-Type': 'application/json' },
  timeout: SERVICE_TIMEOUTS.python,
});

api.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('auth_token');
    if (token) config.headers.Authorization = `Bearer ${token}`;
    console.log('API Request:', config.method?.toUpperCase(), config.url, config.data);
    return config;
  },
  (error) => Promise.reject(error)
);

api.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.response?.status === 401) {
      localStorage.removeItem('auth_token');
      localStorage.removeItem('auth_user');
      window.location.href = '/login';
    }
    return Promise.reject({
      message: error.message,
      status: error.response?.status,
      statusText: error.response?.statusText,
      data: error.response?.data,
      config: error.config,
    });
  }
);

pythonApi.interceptors.request.use(
  (config) => {
    console.log('Python API Request:', config.method?.toUpperCase(), config.url, config.data);
    return config;
  },
  (error) => Promise.reject(error)
);

pythonApi.interceptors.response.use(
  (response) => response.data,
  (error) => Promise.reject({
    message: error.message,
    status: error.response?.status,
    statusText: error.response?.statusText,
    data: error.response?.data,
    config: error.config,
  })
);

// Distributed C/S service (python_service/server).
const csApi = axios.create({
  baseURL: CS_API_BASE_URL,
  headers: { 'Content-Type': 'application/json' },
  timeout: SERVICE_TIMEOUTS.cs,
});

csApi.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('cs_auth_token');
    if (token) config.headers.Authorization = `Bearer ${token}`;
    console.log('CS API Request:', config.method?.toUpperCase(), config.url, config.data);
    return config;
  },
  (error) => Promise.reject(error)
);

csApi.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.response?.status === 401) localStorage.removeItem('cs_auth_token');
    return Promise.reject({
      message: error.message,
      status: error.response?.status,
      statusText: error.response?.statusText,
      data: error.response?.data,
      config: error.config,
    });
  }
);

export {
  pythonApi,
  csApi,
  CPP_BASE_URL,
  PYTHON_API_BASE_URL,
  PYTHON_API_BASE_URL as PYTHON_BASE_URL,
};
export default api;
