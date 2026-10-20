/**
 * @file crash_handler.h
 * @brief 进程崩溃信号处理与回溯记录
 * @author Christins
 * @date 2026-10-20
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

namespace blog {

/**
 * @brief 安装崩溃信号处理器（SIGSEGV/SIGABRT/SIGBUS/SIGFPE）
 * @param log_path 崩溃日志文件路径
 * @return true 安装成功
 * @note 处理器先写 backtrace 到日志，再恢复默认处理并重新抛出信号，
 *       让 master 仍能感知 worker 崩溃并重启。
 */
bool InstallCrashHandler(const std::string& log_path);

/**
 * @brief 恢复默认信号处理
 * @note 仅在进程退出路径（onUnload）调用；热重载时不要调用，
 *       否则会覆盖新模块安装的处理器。
 */
void UninstallCrashHandler();

} // namespace blog
