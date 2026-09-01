/**
 * 微信取证（WeChat Forensics）服务
 * 与 Python FastAPI 服务 (端口 8090) 的 /api/wechat/forensics/* 通信。
 * 导入来源：解密后（或加密的）安卓微信账号数据库 EnMicroMsg.db。
 */
import { pythonApi, PYTHON_API_BASE_URL } from './api';

/** 创建微信取证导入（传入数据库路径，服务端解密+解析+归一化） */
export const createWeChatImport = async (payload) =>
    pythonApi.post('/api/wechat/forensics/imports', payload);

/** 导入列表 */
export const listWeChatImports = async () =>
    pythonApi.get('/api/wechat/forensics/imports');

/** 单个导入摘要 */
export const getWeChatImport = async (importId) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}`);

/** 删除导入 */
export const deleteWeChatImport = async (importId) =>
    pythonApi.delete(`/api/wechat/forensics/imports/${importId}`);

/** 取证概览（账号/密钥材料/解密参数/统计/消息类型分布/每日活跃） */
export const getWeChatForensicsOverview = async (importId) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}/overview`);

/** 会话列表（每个 talker 一行，含显示名与最后消息预览） */
export const getWeChatSessions = async (importId) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}/sessions`);

/** 消息查询（人类可读字段，支持会话/类型/关键词/时间过滤与分页） */
export const getWeChatMessages = async (importId, params = {}) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}/messages`, { params });

/** 联系人列表（含可读类型：好友/公众号/系统功能账号） */
export const getWeChatContacts = async (importId) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}/contacts`);

/** 群聊列表（含成员显示名） */
export const getWeChatChatrooms = async (importId) =>
    pythonApi.get(`/api/wechat/forensics/imports/${importId}/chatrooms`);

/** 媒体文件完整 URL（图片缩略图等） */
export const wechatMediaUrl = (importId, relPath) =>
    `${PYTHON_API_BASE_URL}/api/wechat/forensics/imports/${importId}/media/${relPath}`;
