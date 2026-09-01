/**
 * QQ 取证（QQ Forensics）服务
 * 与 Python FastAPI 服务 (端口 8090) 的 /api/qq/forensics/* 通信。
 * 导入来源：安卓 QQ (NTQQ) 的 nt_msg.db（SQLCipher 加密或明文）。
 */
import { pythonApi } from './api';

/** 创建 QQ 取证导入（传入数据库路径，服务端解密+解析+归一化） */
export const createQQImport = async (payload) =>
    pythonApi.post('/api/qq/forensics/imports', payload);

/** 导入列表 */
export const listQQImports = async () =>
    pythonApi.get('/api/qq/forensics/imports');

/** 单个导入摘要 */
export const getQQImport = async (importId) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}`);

/** 删除导入 */
export const deleteQQImport = async (importId) =>
    pythonApi.delete(`/api/qq/forensics/imports/${importId}`);

/** 取证概览（账号/密钥材料/解密参数/统计/消息类型分布/每日活跃） */
export const getQQForensicsOverview = async (importId) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}/overview`);

/** 会话列表（私聊对象与群聊，含显示名与最后消息预览） */
export const getQQSessions = async (importId) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}/sessions`);

/** 消息查询（人类可读字段，支持会话/类型/关键词/时间过滤与分页） */
export const getQQMessages = async (importId, params = {}) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}/messages`, { params });

/** 联系人列表（好友/聊天对象） */
export const getQQContacts = async (importId) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}/contacts`);

/** 群聊列表（含群名与成员预览） */
export const getQQChatrooms = async (importId) =>
    pythonApi.get(`/api/qq/forensics/imports/${importId}/chatrooms`);
