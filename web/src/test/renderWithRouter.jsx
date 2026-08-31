import { configureStore } from '@reduxjs/toolkit';
import { render } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import { Provider } from 'react-redux';
import settingsReducer from '../store/settingsSlice';

// 默认带 Redux Provider：被渲染组件普遍通过 useTranslation 读取
// settings.language（如 TaskTable），缺 Provider 会直接抛错。
export function renderWithRouter(ui, { route = '/', store } = {}) {
  window.history.pushState({}, 'Test page', route);
  const reduxStore =
    store ??
    configureStore({
      reducer: { settings: settingsReducer },
    });
  return render(
    <Provider store={reduxStore}>
      <MemoryRouter initialEntries={[route]}>{ui}</MemoryRouter>
    </Provider>,
  );
}
