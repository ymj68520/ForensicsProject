import { csApi } from './api';

// Login uses the OAuth2 password flow -> form-encoded, NOT JSON. The endpoint
// depends on OAuth2PasswordRequestForm; a JSON body 422s. Passing URLSearchParams
// makes axios send application/x-www-form-urlencoded automatically.
export const csLogin = (username, password) => {
  const form = new URLSearchParams();
  form.append('username', username);
  form.append('password', password);
  // csApi 实例默认 Content-Type 是 application/json，会压过浏览器对
  // URLSearchParams 的自动推断，服务端把 body 当 JSON 解析即 422
  // （"Field required: username"）。这里显式指定表单类型。
  return csApi.post('/api/auth/login', form, {
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
  });
};

export const csRefresh = (token) =>
  csApi.post('/api/auth/refresh', {}, { headers: { Authorization: `Bearer ${token}` } });

export const csMe = () => csApi.get('/api/auth/me');
