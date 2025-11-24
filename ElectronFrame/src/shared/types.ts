// 共享的类型定义

export interface AppConfig {
    version: string;
    name: string;
    debug: boolean;
}

export interface Message {
    id: string;
    content: string;
    timestamp: Date;
    type: 'user' | 'system';
}