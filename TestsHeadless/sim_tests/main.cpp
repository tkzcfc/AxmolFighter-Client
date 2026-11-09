#include "doctest.h"

// 新 sim 测试入口。
int main(int argc, char** argv)
{
    return doctest::Context(argc, argv).run();
}
