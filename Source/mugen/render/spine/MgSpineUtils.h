#pragma once

#include "mugen/render/spine/MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

#    include <functional>
#    include <string>
#    include <string_view>
#    include <vector>

NS_MG_BEGIN

std::string atlasFromSpine(std::string_view spine);

// 探测数据是二进制还是json格式
bool checkIsJsonFormat(const ax::Data& data);

// 通过文件内容获取 MgSpineRuntime
MgSpineRuntime runtimeFromSkeletonData(const ax::Data& data);
MgSpineRuntime runtimeFromSkeletonData(const void* bytes, size_t size);

// 收集图集引用的全部页贴图路径（去重）；纯文本 IO，主线程/工作线程均可
std::vector<std::string> collectTexturePaths(const std::vector<std::string>& atlasFiles);

// 主线程：异步解码全部贴图，全部完成（含单张失败）后于主线程 onDone
void prewarmTexturesOnMain(std::vector<std::string> texturePaths, std::function<void()> onDone);

NS_MG_END

#endif
