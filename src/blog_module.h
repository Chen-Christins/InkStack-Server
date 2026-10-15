/**
 * @file blog_module.h
 * @brief 项目模块
 * @author Christins
 * @date 2025-05-10
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/module/module.h>

namespace blog {

class BlogModule : public chen::Module {
public:
    typedef std::shared_ptr<BlogModule> ptr;
    /**
     * @brief 构造函数
     */
    BlogModule();

    /**
     * @brief 模块加载
     * @return bool
     */
    bool onLoad() override;

    /**
     * @brief 模块卸载
     * @return bool
     */
    bool onUnload() override;

    /**
     * @brief 定时器回调
     */
    void onTick() override;

    /**
     * @brief 设置定时器间隔
     * @return uint64_t 
     */
    uint64_t getTickIntervalMs() override;

    /**
     * @brief 服务器就绪
     * @return bool
     */
    bool onServerReady() override;

    /**
     * @brief 服务器启动
     * @return bool
     */
    bool onServerUp() override;

    /**
     * @brief 模块停用：旧模块被替换时调用（热重载）
     * @details 新模块已接管，旧模块停止接收新请求。
     *          用于关闭 WebSocket 连接等长连接。默认实现返回 true。
     * @return bool
     */
    bool onDeactivate() override;
private:

    /**
     * @brief 初始化数据库
     */
    static bool initMySQL();

    /**
     * @brief 注册Servlet
     */
    void registerServlets();

    /**
     * @brief 注册WebSocket Servlet
     */
    void registerWSServlets();

    /**
     * @brief 注册RPC方法
     */
    void registerRPCMethods();
};

}
